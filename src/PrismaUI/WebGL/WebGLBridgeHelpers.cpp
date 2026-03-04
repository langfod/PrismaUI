#include "WebGLBridgeInternal.h"

#include "ANGLEContext.h"
#include "PrismaUI/Core.h"

#include <d3d11.h>
#include <spdlog/spdlog.h>
#include <vector>
#include <chrono>

namespace PrismaUI::WebGL {

    // =========================================================================
    // Per-frame context activation.
    // eglMakeCurrent must run at least once per frame to ensure ANGLE's
    // context is current on the Ultralight thread.  Since ANGLE now has its
    // own D3D11 device, we no longer need to save/restore Skyrim's D3D11
    // render targets — the two devices are independent.
    // =========================================================================
    thread_local bool g_contextActivatedThisFrame = false;

    void EnsureContextActive(ANGLEContext* c) {
        if (!g_contextActivatedThisFrame) {
            eglMakeCurrent(c->eglDisplay, c->eglSurface, c->eglSurface, c->eglContext);
            g_contextActivatedThisFrame = true;
        }
    }

    void ResetFrameState() {
        g_contextActivatedThisFrame = false;
    }

    void EndFrameGLState() {
        // No-op: ANGLE has its own D3D11 device, so no state restoration needed.
        // Kept for API compatibility with Core.cpp.
    }

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
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (now - lastLog > 5000) {
                logger::debug("[WebGL-DBG] ReadbackToSharedTexture: sharedTexture is null");
                lastLog = now;
            }
            return;
        }

        uint32_t w = c->canvasWidth;
        uint32_t h = c->canvasHeight;

        std::vector<GLubyte> pixels(w * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

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
                if (idx + 3 < pixels.size())
                    return fmt::format("({},{},{},{})", pixels[idx], pixels[idx+1], pixels[idx+2], pixels[idx+3]);
                return "(OOB)";
            };
            logger::info("[WebGL-DBG] Pixel samples: [0,0]={} [150,75]={} [299,149]={}",
                px(0, 0), px(w/2, h/2), px(w-1, h-1));

            // Check current framebuffer binding
            GLint fbo = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
            logger::info("[WebGL-DBG] Current FBO at readback time: {}", fbo);
        }

        // Swizzle RGBA -> BGRA and flip vertically.
        // glReadPixels returns rows bottom-to-top (OpenGL convention) but
        // D3D11 textures are top-to-bottom, so we must flip during the copy.
        // Force alpha to 255: the default WebGL framebuffer is opaque, but
        // ANGLE can return non-255 alpha values depending on the pbuffer
        // surface configuration and blend state at clear/draw time.
        uint32_t rowBytes = w * 4;
        std::vector<GLubyte> flipped(w * h * 4);
        for (uint32_t row = 0; row < h; row++) {
            GLubyte* src = pixels.data() + row * rowBytes;
            GLubyte* dst = flipped.data() + (h - 1 - row) * rowBytes;
            for (uint32_t i = 0; i < rowBytes; i += 4) {
                dst[i + 0] = src[i + 2]; // B <- R
                dst[i + 1] = src[i + 1]; // G
                dst[i + 2] = src[i + 0]; // R <- B
                dst[i + 3] = 255;        // Force opaque
            }
        }

        ID3D11DeviceContext* d3dCtx = PrismaUI::Core::d3dContext;
        if (!d3dCtx) {
            static uint64_t lastLog = 0;
            uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (now - lastLog > 5000) {
                logger::debug("[WebGL-DBG] ReadbackToSharedTexture: d3dContext is null — pixels lost!");
                lastLog = now;
            }
            return;
        }

        D3D11_BOX box = {0, 0, 0, w, h, 1};
        d3dCtx->UpdateSubresource(c->sharedTexture.Get(), 0, &box, flipped.data(), rowBytes, 0);

        // One-time log to confirm readback is working
        static bool loggedOnce = false;
        if (!loggedOnce) {
            loggedOnce = true;
            logger::info("[WebGL-DBG] ReadbackToSharedTexture: first successful readback {}x{}, rowPitch={}", w, h, rowBytes);

            // Verify the shared texture dimensions match
            D3D11_TEXTURE2D_DESC desc = {};
            c->sharedTexture->GetDesc(&desc);
            logger::info("[WebGL-DBG] SharedTexture desc: {}x{} format={} usage={} bindFlags=0x{:X}",
                desc.Width, desc.Height, static_cast<int>(desc.Format),
                static_cast<int>(desc.Usage), desc.BindFlags);

            // Check a pixel in the flipped/swizzled buffer
            if (flipped.size() >= 4) {
                logger::info("[WebGL-DBG] Flipped buffer[0..3] (BGRA): {},{},{},{}",
                    flipped[0], flipped[1], flipped[2], flipped[3]);
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
                if (idx + 3 < pixels.size())
                    return fmt::format("({},{},{},{})", pixels[idx], pixels[idx+1], pixels[idx+2], pixels[idx+3]);
                return "(OOB)";
            };
            logger::info("[WebGL-DBG] Readback frame 30 pixels (RGBA): [0,0]={} [80,72]={} [150,75]={}",
                px(0, 0), px(80, 72), px(w/2, h/2));
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
