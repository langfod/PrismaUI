#include "WebGLBridgeInternal.h"

#include <spdlog/spdlog.h>
#include <vector>

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
                    void* ptr = GetTypedArrayDataPtr(ctx, dataObj);
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

        static bool loggedFirstUpload = false;
        if (!loggedFirstUpload) {
            logger::info("[WebGL-DBG] GL_texSubImage2D: first upload {}x{} format=0x{:X} type=0x{:X}",
                width, height, format, type);
            loggedFirstUpload = true;
        }

        if (argc > 8 && JSValueIsObject(ctx, argv[8]) && !JSValueIsNull(ctx, argv[8])) {
            JSObjectRef dataObj = JSValueToObject(ctx, argv[8], nullptr);
            void* ptr = JSObjectGetTypedArrayBytesPtr(ctx, dataObj, nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, dataObj, nullptr);

            // Workaround: JSObjectGetTypedArrayBytesPtr in some JSC builds may
            // return the ArrayBuffer base without adding the TypedArray's byte
            // offset.  Detect this and correct the pointer if needed.
            size_t taByteOffset = JSObjectGetTypedArrayByteOffset(ctx, dataObj, nullptr);
            JSObjectRef backingAB = JSObjectGetTypedArrayBuffer(ctx, dataObj, nullptr);
            void* abPtr = backingAB ? JSObjectGetArrayBufferBytesPtr(ctx, backingAB, nullptr) : nullptr;

            if (abPtr && taByteOffset > 0 && ptr == abPtr) {
                // Byte offset was NOT incorporated — fix it
                ptr = static_cast<uint8_t*>(abPtr) + taByteOffset;
                static bool loggedOffsetFix = false;
                if (!loggedOffsetFix) {
                    logger::info("[WebGL-DBG] texSubImage2D: fixed TypedArray ptr (ABbase={}, +offset={} -> {})",
                        abPtr, taByteOffset, ptr);
                    loggedOffsetFix = true;
                }
            }

            // Log details of the first few uploads to diagnose data issues
            static int uploadCount = 0;
            uploadCount++;
            if (uploadCount <= 5) {
                auto* bytes = static_cast<uint8_t*>(ptr);
                bool allZero = true;
                uint8_t sample[4] = {};
                if (ptr && byteLen >= 4) {
                    // Sample from middle of data
                    size_t mid = (byteLen / 2) & ~3;
                    sample[0] = bytes[mid]; sample[1] = bytes[mid+1];
                    sample[2] = bytes[mid+2]; sample[3] = bytes[mid+3];
                    // Check ALL bytes, not just first 1024
                    for (size_t i = 0; i < byteLen; i++) {
                        if (bytes[i] != 0) { allZero = false; break; }
                    }
                }

                // Also check directly from ArrayBuffer base + offset
                bool abAllZero = true;
                uint8_t abSample[4] = {};
                if (abPtr && taByteOffset + byteLen <= JSObjectGetArrayBufferByteLength(ctx, backingAB, nullptr)) {
                    auto* abBytes = static_cast<uint8_t*>(abPtr) + taByteOffset;
                    size_t mid = (byteLen / 2) & ~3;
                    abSample[0] = abBytes[mid]; abSample[1] = abBytes[mid+1];
                    abSample[2] = abBytes[mid+2]; abSample[3] = abBytes[mid+3];
                    for (size_t i = 0; i < byteLen && i < 4096; i++) {
                        if (abBytes[i] != 0) { abAllZero = false; break; }
                    }
                }

                logger::info("[WebGL-DBG] texSubImage2D #{}: TAptr={} TAbyteOff={} ABptr={} TAptr==AB+off? {} byteLen={} allZero={} mid=({},{},{},{}) AB+off_allZero={} AB+off_mid=({},{},{},{}) GL_err=0x{:X}",
                    uploadCount, ptr, taByteOffset, abPtr,
                    (ptr == static_cast<uint8_t*>(abPtr) + taByteOffset),
                    byteLen, allZero,
                    sample[0], sample[1], sample[2], sample[3],
                    abAllZero,
                    abSample[0], abSample[1], abSample[2], abSample[3],
                    glGetError());
            }

            glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, ptr);

            if (uploadCount <= 5) {
                GLenum err = glGetError();
                if (err != GL_NO_ERROR) {
                    logger::warn("[WebGL-DBG] texSubImage2D #{}: GL error AFTER upload: 0x{:X}", uploadCount, err);
                }
            }
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
            void* ptr = GetTypedArrayDataPtr(ctx, dataObj);
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

        static bool loggedFirstDraw = false;
        if (!loggedFirstDraw) {
            logger::info("[WebGL-DBG] GL_drawArrays: first draw call (mode={}, first={}, count={})",
                static_cast<int>(JSValueToNumber(ctx, argv[0], nullptr)),
                static_cast<int>(JSValueToNumber(ctx, argv[1], nullptr)),
                static_cast<int>(JSValueToNumber(ctx, argv[2], nullptr)));
            loggedFirstDraw = true;
        }

        glDrawArrays(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[2], nullptr)));
        c->frameDirty = true;
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
        c->frameDirty = true;

        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Instanced Drawing (WebGL2)
    // =========================================================================

    JSValueRef GL_drawArraysInstanced(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glDrawArraysInstanced(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr)));
        c->frameDirty = true;
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_drawElementsInstanced(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                         size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);

        GLenum mode = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLsizei count = static_cast<GLsizei>(JSValueToNumber(ctx, argv[1], nullptr));
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr));
        auto offset = static_cast<intptr_t>(JSValueToNumber(ctx, argv[3], nullptr));
        GLsizei instanceCount = static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr));

        glDrawElementsInstanced(mode, count, type, reinterpret_cast<const void*>(offset), instanceCount);
        c->frameDirty = true;

        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_drawRangeElements(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 6) return JSValueMakeUndefined(ctx);

        GLenum mode = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint start = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLuint end = static_cast<GLuint>(JSValueToNumber(ctx, argv[2], nullptr));
        GLsizei count = static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr));
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[4], nullptr));
        auto offset = static_cast<intptr_t>(JSValueToNumber(ctx, argv[5], nullptr));

        glDrawRangeElements(mode, start, end, count, type, reinterpret_cast<const void*>(offset));
        c->frameDirty = true;

        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttribDivisor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glVertexAttribDivisor(
            static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // 3D Textures & Storage (WebGL2)
    // =========================================================================

    JSValueRef GL_texStorage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        glTexStorage2D(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_texStorage3D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 6) return JSValueMakeUndefined(ctx);
        glTexStorage3D(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[5], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_texImage3D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 10) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint level = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint internalformat = static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr));
        GLsizei width = static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr));
        GLsizei height = static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr));
        GLsizei depth = static_cast<GLsizei>(JSValueToNumber(ctx, argv[5], nullptr));
        GLint border = static_cast<GLint>(JSValueToNumber(ctx, argv[6], nullptr));
        GLenum format = static_cast<GLenum>(JSValueToNumber(ctx, argv[7], nullptr));
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[8], nullptr));

        const void* data = nullptr;
        if (argc > 9 && JSValueIsObject(ctx, argv[9]) &&
            !JSValueIsNull(ctx, argv[9]) && !JSValueIsUndefined(ctx, argv[9])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[9], nullptr);
            data = GetTypedArrayDataPtr(ctx, arr);
        }
        glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, data);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_texSubImage3D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 11) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint level = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint xoffset = static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr));
        GLint yoffset = static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr));
        GLint zoffset = static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr));
        GLsizei width = static_cast<GLsizei>(JSValueToNumber(ctx, argv[5], nullptr));
        GLsizei height = static_cast<GLsizei>(JSValueToNumber(ctx, argv[6], nullptr));
        GLsizei depth = static_cast<GLsizei>(JSValueToNumber(ctx, argv[7], nullptr));
        GLenum format = static_cast<GLenum>(JSValueToNumber(ctx, argv[8], nullptr));
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[9], nullptr));

        const void* data = nullptr;
        if (argc > 10 && JSValueIsObject(ctx, argv[10]) &&
            !JSValueIsNull(ctx, argv[10]) && !JSValueIsUndefined(ctx, argv[10])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[10], nullptr);
            data = GetTypedArrayDataPtr(ctx, arr);
        }
        glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, data);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_copyTexSubImage3D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 9) return JSValueMakeUndefined(ctx);
        glCopyTexSubImage3D(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[5], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[6], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[7], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[8], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Framebuffer Enhancements (WebGL2)
    // =========================================================================

    JSValueRef GL_drawBuffers(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);

        JSObjectRef arr = JSValueToObject(ctx, argv[0], nullptr);
        if (!arr) return JSValueMakeUndefined(ctx);

        JSStringRef lengthKey = JSStringCreateWithUTF8CString("length");
        GLsizei count = static_cast<GLsizei>(
            JSValueToNumber(ctx, JSObjectGetProperty(ctx, arr, lengthKey, nullptr), nullptr));
        JSStringRelease(lengthKey);

        std::vector<GLenum> bufs(count);
        for (GLsizei i = 0; i < count; i++) {
            JSValueRef elem = JSObjectGetPropertyAtIndex(ctx, arr, i, nullptr);
            bufs[i] = static_cast<GLenum>(JSValueToNumber(ctx, elem, nullptr));
        }
        glDrawBuffers(count, bufs.data());
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_readBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glReadBuffer(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_blitFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 10) return JSValueMakeUndefined(ctx);
        glBlitFramebuffer(
            static_cast<GLint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[5], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[6], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[7], nullptr)),
            static_cast<GLbitfield>(JSValueToNumber(ctx, argv[8], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[9], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_framebufferTextureLayer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum attachment = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLuint texture = 0;
        if (!JSValueIsNull(ctx, argv[2]) && !JSValueIsUndefined(ctx, argv[2]))
            texture = GetGLId(ctx, argv[2]);
        GLint level = static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr));
        GLint layer = static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr));
        glFramebufferTextureLayer(target, attachment, texture, level, layer);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_renderbufferStorageMultisample(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        glRenderbufferStorageMultisample(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_invalidateFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                         size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));

        JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
        if (!arr) return JSValueMakeUndefined(ctx);

        JSStringRef lengthKey = JSStringCreateWithUTF8CString("length");
        GLsizei count = static_cast<GLsizei>(
            JSValueToNumber(ctx, JSObjectGetProperty(ctx, arr, lengthKey, nullptr), nullptr));
        JSStringRelease(lengthKey);

        std::vector<GLenum> attachments(count);
        for (GLsizei i = 0; i < count; i++) {
            JSValueRef elem = JSObjectGetPropertyAtIndex(ctx, arr, i, nullptr);
            attachments[i] = static_cast<GLenum>(JSValueToNumber(ctx, elem, nullptr));
        }
        glInvalidateFramebuffer(target, count, attachments.data());
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_invalidateSubFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 6) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));

        JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
        if (!arr) return JSValueMakeUndefined(ctx);

        JSStringRef lengthKey = JSStringCreateWithUTF8CString("length");
        GLsizei count = static_cast<GLsizei>(
            JSValueToNumber(ctx, JSObjectGetProperty(ctx, arr, lengthKey, nullptr), nullptr));
        JSStringRelease(lengthKey);

        std::vector<GLenum> attachments(count);
        for (GLsizei i = 0; i < count; i++) {
            JSValueRef elem = JSObjectGetPropertyAtIndex(ctx, arr, i, nullptr);
            attachments[i] = static_cast<GLenum>(JSValueToNumber(ctx, elem, nullptr));
        }

        GLint x = static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr));
        GLint y = static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr));
        GLsizei width = static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr));
        GLsizei height = static_cast<GLsizei>(JSValueToNumber(ctx, argv[5], nullptr));
        glInvalidateSubFramebuffer(target, count, attachments.data(), x, y, width, height);
        return JSValueMakeUndefined(ctx);
    }

}  // namespace PrismaUI::WebGL
