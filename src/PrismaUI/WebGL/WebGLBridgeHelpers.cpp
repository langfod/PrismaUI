#include <d3d11.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <vector>

#include "ANGLEContext.h"
#include "PrismaUI/Core.h"
#include "Utils/SIMDDispatch.h"
#include "WebGLBridgeInternal.h"


namespace PrismaUI::WebGL {

    // =========================================================================
    // Per-frame context activation.
    // eglMakeCurrent is called whenever the active context changes on this
    // thread — once per canvas per frame in the typical single-canvas case,
    // and on every cross-canvas switch in multi-canvas frames.
    //
    // g_currentContext tracks which ANGLEContext is currently bound.
    // nullptr at frame start (cleared by ResetFrameState).  Comparing by
    // pointer avoids redundant eglMakeCurrent calls on single-canvas frames
    // while correctly switching when multiple canvases interleave GL calls.
    // =========================================================================
    thread_local ANGLEContext* g_currentContext = nullptr;

    void EnsureContextActive(ANGLEContext* c) {
        if (g_currentContext != c) {
            // Acquire the ANGLE-side keyed mutex for this context if needed.
            // Guard with angleMutexAcquired so we never double-acquire when
            // switching back to a context that was already set up this frame.
            // FlushDirtyContexts releases it (key 1) at end of frame.
            if (c->useSharedTexturePath && c->angleMutex && !c->angleMutexAcquired) {
                // 16ms timeout = one full frame at 60 FPS; the render thread
                // typically releases within <1ms.
                HRESULT hr = c->angleMutex->AcquireSync(0, 16);
                if (SUCCEEDED(hr)) {
                    c->angleMutexAcquired = true;
                } else {
                    c->angleMutexAcquired = false;
                    static uint64_t lastLog = 0;
                    static uint32_t droppedFrames = 0;
                    ++droppedFrames;
                    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now().time_since_epoch())
                                       .count();
                    if (now - lastLog > 5000) {
                        logger::warn("[WebGL] ANGLE keyed mutex acquire failed: 0x{:X} ({} dropped frames total)",
                                     static_cast<uint32_t>(hr), droppedFrames);
                        lastLog = now;
                    }
                }
            }
            if (!eglMakeCurrent(c->eglDisplay, c->eglSurface, c->eglSurface, c->eglContext)) {
                logger::error("[WebGL] eglMakeCurrent failed: 0x{:X}", eglGetError());
                return;
            }
            g_currentContext = c;
        }
    }

    void ResetFrameState() { g_currentContext = nullptr; }

    // =========================================================================
    // Helper: extract ANGLEContext from JSC thisObject's private data.
    // Also ensures the EGL context is active (once per frame) so that all
    // subsequent GL calls in this frame operate on a valid context.
    // =========================================================================
    ANGLEContext* GetContext(JSObjectRef thisObject) {
        auto* c = static_cast<ANGLEContext*>(JSObjectGetPrivate(thisObject));
        if (c && c->initialized) {
            EnsureContextActive(c);
        }
        return c;
    }

    // =========================================================================
    // Readback: copy ANGLE's rendered content into the shared D3D11 texture
    // for compositing.  ANGLE runs on its own D3D11 device, so we must
    // glReadPixels the result and UpdateSubresource it into sharedTexture
    // (which lives on Skyrim's D3D11 device).
    // =========================================================================
    void ReadbackToSharedTexture(ANGLEContext* c) {
        if (!c->sharedTexture) {
            static uint64_t lastLog = 0;
            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
            if (now - lastLog > 5000) {
                logger::debug("[WebGL-DBG] ReadbackToSharedTexture: sharedTexture is null");
                lastLog = now;
            }
            return;
        }

        uint32_t w = c->canvasWidth;
        uint32_t h = c->canvasHeight;

        // Ensure persistent buffers are large enough (safety check for edge cases)
        size_t requiredBytes = static_cast<size_t>(w) * h * 4;
        if (c->readbackPixels.size() < requiredBytes) {
            c->readbackPixels.resize(requiredBytes);
            c->readbackFlipped.resize(requiredBytes);
        }

        GLubyte* pixels = c->readbackPixels.data();
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        // Log the first readback's contents to diagnose garbled output
        static bool loggedPixels = false;
        if (!loggedPixels) {
            loggedPixels = true;
            // Check GL error after readPixels
            GLenum err = glGetError();
            logger::info("[WebGL-DBG] ReadbackToSharedTexture: glReadPixels {}x{} err=0x{:X}", w, h, err);
            // Sample some pixels: top-left, center, and bottom-right
            auto px = [&](uint32_t x, uint32_t y) -> std::string {
                uint32_t idx = (y * w + x) * 4;
                if (idx + 3 < requiredBytes)
                    return fmt::format("({},{},{},{})", pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]);
                return "(OOB)";
            };

            // Check current framebuffer binding
            GLint fbo = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
            logger::info("[WebGL-DBG] Current FBO at readback time: {}", fbo);
        }

        // Swizzle RGBA -> BGRA, force alpha to 255, and flip vertically.
        // glReadPixels returns rows bottom-to-top (OpenGL convention) but
        // D3D11 textures are top-to-bottom, so we must flip during the copy.
        GLubyte* flipped = c->readbackFlipped.data();
        SIMD::SwizzleFlipPixels(flipped, pixels, w, h);

        ID3D11DeviceContext* d3dCtx = PrismaUI::Core::d3dContext;
        if (!d3dCtx) {
            static uint64_t lastLog = 0;
            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();
            if (now - lastLog > 5000) {
                logger::debug("[WebGL-DBG] ReadbackToSharedTexture: d3dContext is null — pixels lost!");
                lastLog = now;
            }
            return;
        }

        D3D11_BOX box = {0, 0, 0, w, h, 1};
        uint32_t rowBytes = w * 4;
        d3dCtx->UpdateSubresource(c->sharedTexture.Get(), 0, &box, flipped, rowBytes, 0);

        // One-time log to confirm readback is working
        static bool loggedOnce = false;
        if (!loggedOnce) {
            loggedOnce = true;
            logger::info("[WebGL-DBG] ReadbackToSharedTexture: first successful readback {}x{}, rowPitch={}", w, h,
                         rowBytes);

            // Verify the shared texture dimensions match
            D3D11_TEXTURE2D_DESC desc = {};
            c->sharedTexture->GetDesc(&desc);
            logger::info("[WebGL-DBG] SharedTexture desc: {}x{} format={} usage={} bindFlags=0x{:X}", desc.Width,
                         desc.Height, static_cast<int>(desc.Format), static_cast<int>(desc.Usage), desc.BindFlags);

            // Check a pixel in the flipped/swizzled buffer
            if (requiredBytes >= 4) {
                logger::info("[WebGL-DBG] Flipped buffer[0..3] (BGRA): {},{},{},{}", flipped[0], flipped[1], flipped[2],
                             flipped[3]);
            }

            // Log GL viewport
            GLint vp[4] = {};
            glGetIntegerv(GL_VIEWPORT, vp);
            logger::info("[WebGL-DBG] GL viewport: x={} y={} w={} h={}", vp[0], vp[1], vp[2], vp[3]);
        }

        // Second pixel check after several frames to verify real content
        static int readbackCount = 0;
        readbackCount++;
        if (readbackCount == 30) {
            auto px = [&](uint32_t px, uint32_t py) -> std::string {
                uint32_t idx = (py * w + px) * 4;
                if (idx + 3 < requiredBytes)
                    return fmt::format("({},{},{},{})", pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]);
                return "(OOB)";
            };
            logger::info("[WebGL-DBG] Readback frame 30 pixels (RGBA): [0,0]={} [80,72]={} [{},{}]={}", px(0, 0),
                         px(80, 72), w / 2, h / 2, px(w / 2, h / 2));
        }
    }

    // =========================================================================
    // Deferred end-of-frame flush.
    // Called once per frame from Core.cpp after renderer->Render() and before
    // the render thread composites.
    //
    // Shared texture path:  glFlush then release ANGLE keyed mutex (key 1)
    //   so the render thread can acquire it for reading.
    //
    // CPU readback fallback: glFlush then glReadPixels then swizzle then UpdateSubresource.
    // =========================================================================
    void FlushDirtyContexts() {
        for (ANGLEContext* c : GetActiveContexts()) {
            if (!c || !c->initialized) continue;

            // Reset per-frame flag — DrawViews checks this to know if
            // keyed mutex synchronization is needed.
            c->mutexReleasedThisFrame = false;

            if (!c->frameDirty) continue;

            EnsureContextActive(c);
            glFlush();

            if (c->useSharedTexturePath) {
                // Release the ANGLE-side keyed mutex so Skyrim can read.
                // Only release if we successfully acquired it this frame.
                if (c->angleMutexAcquired && c->angleMutex) {
                    HRESULT hr = c->angleMutex->ReleaseSync(1);
                    if (FAILED(hr)) {
                        logger::error("[WebGL] Failed to release ANGLE keyed mutex: 0x{:X}", static_cast<uint32_t>(hr));
                    } else {
                        c->mutexReleasedThisFrame = true;
                    }
                    c->angleMutexAcquired = false;
                }
            } else {
                ReadbackToSharedTexture(c);
            }

            c->frameDirty = false;
        }
    }

    // =========================================================================
    // Helper: extract GLuint _id from a WebGL wrapper object (WebGLBuffer, etc.)
    // =========================================================================
    GLuint GetGLId(JSContextRef ctx, JSValueRef val) {
        if (JSValueIsNull(ctx, val) || JSValueIsUndefined(ctx, val)) {
            return 0;
        }
        JSObjectRef obj = JSValueToObject(ctx, val, nullptr);
        if (!obj) return 0;
        JSStringRef idKey = JSStringCreateWithUTF8CString("_id");
        JSValueRef idVal = JSObjectGetProperty(ctx, obj, idKey, nullptr);
        JSStringRelease(idKey);
        if (JSValueIsUndefined(ctx, idVal) || JSValueIsNull(ctx, idVal)) return 0;
        return static_cast<GLuint>(JSValueToNumber(ctx, idVal, nullptr));
    }

    // =========================================================================
    // Helper: create a WebGL wrapper object (e.g., new WebGLBuffer(id))
    // =========================================================================
    JSValueRef MakeGLObject(JSContextRef ctx, const char* className, GLuint id) {
        JSObjectRef global = JSContextGetGlobalObject(ctx);
        JSStringRef classStr = JSStringCreateWithUTF8CString(className);
        JSValueRef classVal = JSObjectGetProperty(ctx, global, classStr, nullptr);
        JSStringRelease(classStr);

        if (!JSValueIsObject(ctx, classVal)) {
            return JSValueMakeNull(ctx);
        }
        JSObjectRef classObj = JSValueToObject(ctx, classVal, nullptr);
        JSValueRef arg = JSValueMakeNumber(ctx, static_cast<double>(id));
        JSObjectRef result = JSObjectCallAsConstructor(ctx, classObj, 1, &arg, nullptr);
        return result ? result : JSValueMakeNull(ctx);
    }

    // =========================================================================
    // Helper: extract a C string from JSValueRef
    // =========================================================================
    std::string GetString(JSContextRef ctx, JSValueRef val) {
        if (JSValueIsNull(ctx, val) || JSValueIsUndefined(ctx, val)) return "";
        JSStringRef jsStr = JSValueToStringCopy(ctx, val, nullptr);
        if (!jsStr) return "";
        size_t maxLen = JSStringGetMaximumUTF8CStringSize(jsStr);
        std::string result(maxLen, '\0');
        JSStringGetUTF8CString(jsStr, result.data(), maxLen);
        JSStringRelease(jsStr);
        result.resize(std::strlen(result.c_str()));
        return result;
    }

}  // namespace PrismaUI::WebGL
