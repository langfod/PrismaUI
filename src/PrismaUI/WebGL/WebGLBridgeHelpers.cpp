#include "WebGLBridgeInternal.h"

#include "ANGLEContext.h"
#include "PrismaUI/Core.h"

#include <d3d11.h>
#include <spdlog/spdlog.h>
#include <vector>

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
        if (!c->sharedTexture) return;

        uint32_t w = c->canvasWidth;
        uint32_t h = c->canvasHeight;

        std::vector<GLubyte> pixels(w * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // Swizzle RGBA -> BGRA (glReadPixels gives RGBA, shared texture is B8G8R8A8)
        for (size_t i = 0; i < pixels.size(); i += 4) {
            std::swap(pixels[i], pixels[i + 2]);
        }

        ID3D11DeviceContext* d3dCtx = PrismaUI::Core::d3dContext;
        if (!d3dCtx) return;

        D3D11_BOX box = {0, 0, 0, w, h, 1};
        d3dCtx->UpdateSubresource(c->sharedTexture.Get(), 0, &box, pixels.data(), w * 4, 0);
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
