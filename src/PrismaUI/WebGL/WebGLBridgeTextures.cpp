#include "WebGLBridgeInternal.h"

namespace PrismaUI::WebGL {

    // =========================================================================
    // Textures
    // =========================================================================

    JSValueRef GL_createTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint tex = 0;
        glGenTextures(1, &tex);
        return MakeGLObject(ctx, "WebGLTexture", tex);
    }

    JSValueRef GL_deleteTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint tex = GetGLId(ctx, argv[0]);
        if (tex) glDeleteTextures(1, &tex);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint tex = GetGLId(ctx, argv[1]);
        glBindTexture(target, tex);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_activeTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glActiveTexture(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_texParameteri(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glTexParameteri(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_texParameterf(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glTexParameterf(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_generateMipmap(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glGenerateMipmap(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_texImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 6) return JSValueMakeUndefined(ctx);

        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint level = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLenum internalformat = static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr));

        if (argc >= 9) {
            // Full signature: texImage2D(target, level, internalformat, width, height, border, format, type, data)
            GLsizei width = static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr));
            GLsizei height = static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr));
            // argv[5] = border (ignored, always 0)
            GLenum format = static_cast<GLenum>(JSValueToNumber(ctx, argv[6], nullptr));
            GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[7], nullptr));

            if (argc > 8 && JSValueIsObject(ctx, argv[8]) && !JSValueIsNull(ctx, argv[8])) {
                JSObjectRef dataObj = JSValueToObject(ctx, argv[8], nullptr);
                JSTypedArrayType arrType = JSValueGetTypedArrayType(ctx, argv[8], nullptr);
                if (arrType != kJSTypedArrayTypeNone) {
                    void* ptr = JSObjectGetTypedArrayBytesPtr(ctx, dataObj, nullptr);
                    glTexImage2D(target, level, internalformat, width, height, 0, format, type, ptr);
                } else {
                    glTexImage2D(target, level, internalformat, width, height, 0, format, type, nullptr);
                }
            } else {
                glTexImage2D(target, level, internalformat, width, height, 0, format, type, nullptr);
            }
        }
        // Short signature (with HTMLImageElement/canvas) not supported in Phase 1
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_texSubImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 9) return JSValueMakeUndefined(ctx);

        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint level = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint xoffset = static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr));
        GLint yoffset = static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr));
        GLsizei width = static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr));
        GLsizei height = static_cast<GLsizei>(JSValueToNumber(ctx, argv[5], nullptr));
        GLenum format = static_cast<GLenum>(JSValueToNumber(ctx, argv[6], nullptr));
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[7], nullptr));

        if (argc > 8 && JSValueIsObject(ctx, argv[8]) && !JSValueIsNull(ctx, argv[8])) {
            JSObjectRef dataObj = JSValueToObject(ctx, argv[8], nullptr);
            void* ptr = JSObjectGetTypedArrayBytesPtr(ctx, dataObj, nullptr);
            glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, ptr);
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_copyTexImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 8) return JSValueMakeUndefined(ctx);
        glCopyTexImage2D(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[5], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[6], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[7], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_copyTexSubImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 8) return JSValueMakeUndefined(ctx);
        glCopyTexSubImage2D(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[5], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[6], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[7], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_readPixels(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 7) return JSValueMakeUndefined(ctx);
        GLint x = static_cast<GLint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint y = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLsizei width = static_cast<GLsizei>(JSValueToNumber(ctx, argv[2], nullptr));
        GLsizei height = static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr));
        GLenum format = static_cast<GLenum>(JSValueToNumber(ctx, argv[4], nullptr));
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[5], nullptr));
        if (JSValueIsObject(ctx, argv[6]) && !JSValueIsNull(ctx, argv[6])) {
            JSObjectRef dataObj = JSValueToObject(ctx, argv[6], nullptr);
            void* ptr = JSObjectGetTypedArrayBytesPtr(ctx, dataObj, nullptr);
            if (ptr) {
                glReadPixels(x, y, width, height, format, type, ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Framebuffers
    // =========================================================================

    JSValueRef GL_createFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        return MakeGLObject(ctx, "WebGLFramebuffer", fbo);
    }

    JSValueRef GL_deleteFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint fbo = GetGLId(ctx, argv[0]);
        if (fbo) glDeleteFramebuffers(1, &fbo);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint fbo = GetGLId(ctx, argv[1]);
        glBindFramebuffer(target, fbo);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_framebufferTexture2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        glFramebufferTexture2D(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            GetGLId(ctx, argv[3]),
            static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_checkFramebufferStatus(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                         size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeNumber(ctx, 0);
        GLenum result = glCheckFramebufferStatus(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeNumber(ctx, static_cast<double>(result));
    }

    JSValueRef GL_framebufferRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                          size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glFramebufferRenderbuffer(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            GetGLId(ctx, argv[3]));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Renderbuffers
    // =========================================================================

    JSValueRef GL_createRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint rbo = 0;
        glGenRenderbuffers(1, &rbo);
        return MakeGLObject(ctx, "WebGLRenderbuffer", rbo);
    }

    JSValueRef GL_deleteRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint rbo = GetGLId(ctx, argv[0]);
        if (rbo) glDeleteRenderbuffers(1, &rbo);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glBindRenderbuffer(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            GetGLId(ctx, argv[1]));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_renderbufferStorage(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glRenderbufferStorage(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Drawing
    // =========================================================================

    JSValueRef GL_drawArrays(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glDrawArrays(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[2], nullptr)));
        glFlush();
        ReadbackToSharedTexture(c);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_drawElements(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);

        GLenum mode = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLsizei count = static_cast<GLsizei>(JSValueToNumber(ctx, argv[1], nullptr));
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr));
        auto offset = static_cast<intptr_t>(JSValueToNumber(ctx, argv[3], nullptr));

        glDrawElements(mode, count, type, reinterpret_cast<const void*>(offset));
        glFlush();
        ReadbackToSharedTexture(c);

        return JSValueMakeUndefined(ctx);
    }

}  // namespace PrismaUI::WebGL
