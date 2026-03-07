#include "WebGLBridgeInternal.h"

namespace PrismaUI::WebGL {

    // =========================================================================
    // Buffers
    // =========================================================================

    JSValueRef GL_createBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint buf = 0;
        glGenBuffers(1, &buf);
        return MakeGLObject(ctx, "WebGLBuffer", buf);
    }

    JSValueRef GL_deleteBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint buf = GetGLId(ctx, argv[0]);
        if (buf) glDeleteBuffers(1, &buf);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint buf = GetGLId(ctx, argv[1]);
        glBindBuffer(target, buf);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bufferData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum usage = static_cast<GLenum>(JSValueToNumber(ctx, argv[argc - 1], nullptr));

        // Check if second arg is a size (number) or data (typed array/ArrayBuffer)
        if (JSValueIsNumber(ctx, argv[1])) {
            GLsizeiptr size = static_cast<GLsizeiptr>(JSValueToNumber(ctx, argv[1], nullptr));
            glBufferData(target, size, nullptr, usage);
        } else if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef dataObj = JSValueToObject(ctx, argv[1], nullptr);
            JSTypedArrayType arrType = JSValueGetTypedArrayType(ctx, argv[1], nullptr);
            if (arrType != kJSTypedArrayTypeNone) {
                size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, dataObj, nullptr);
                void* ptr = GetTypedArrayDataPtr(ctx, dataObj);
                glBufferData(target, static_cast<GLsizeiptr>(byteLen), ptr, usage);
            } else {
                // Might be an ArrayBuffer
                size_t byteLen = JSObjectGetArrayBufferByteLength(ctx, dataObj, nullptr);
                if (byteLen > 0) {
                    void* ptr = JSObjectGetArrayBufferBytesPtr(ctx, dataObj, nullptr);
                    glBufferData(target, static_cast<GLsizeiptr>(byteLen), ptr, usage);
                } else {
                    glBufferData(target, 0, nullptr, usage);
                }
            }
        } else {
            glBufferData(target, 0, nullptr, usage);
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bufferSubData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLintptr offset = static_cast<GLintptr>(JSValueToNumber(ctx, argv[1], nullptr));

        if (JSValueIsObject(ctx, argv[2])) {
            JSObjectRef dataObj = JSValueToObject(ctx, argv[2], nullptr);
            JSTypedArrayType arrType = JSValueGetTypedArrayType(ctx, argv[2], nullptr);
            if (arrType != kJSTypedArrayTypeNone) {
                size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, dataObj, nullptr);
                void* ptr = GetTypedArrayDataPtr(ctx, dataObj);
                if (ptr && byteLen > 0) {
                    glBufferSubData(target, offset, static_cast<GLsizeiptr>(byteLen), ptr);
                }
            } else {
                // Plain ArrayBuffer
                size_t byteLen = JSObjectGetArrayBufferByteLength(ctx, dataObj, nullptr);
                if (byteLen > 0) {
                    void* ptr = JSObjectGetArrayBufferBytesPtr(ctx, dataObj, nullptr);
                    glBufferSubData(target, offset, static_cast<GLsizeiptr>(byteLen), ptr);
                }
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Buffer Operations (WebGL2)
    // =========================================================================

    JSValueRef GL_copyBufferSubData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        glCopyBufferSubData(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLintptr>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLintptr>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLsizeiptr>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_getBufferSubData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLintptr srcByteOffset = static_cast<GLintptr>(JSValueToNumber(ctx, argv[1], nullptr));

        if (!JSValueIsObject(ctx, argv[2])) return JSValueMakeUndefined(ctx);
        JSObjectRef dstData = JSValueToObject(ctx, argv[2], nullptr);
        size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, dstData, nullptr);
        void* dstPtr = GetTypedArrayDataPtr(ctx, dstData);
        if (!dstPtr || byteLen == 0) return JSValueMakeUndefined(ctx);

        // Validate range against the GPU buffer size before mapping
        GLint bufferSize = 0;
        glGetBufferParameteriv(target, GL_BUFFER_SIZE, &bufferSize);
        if (bufferSize <= 0 ||
            srcByteOffset < 0 ||
            static_cast<GLintptr>(byteLen) > bufferSize - srcByteOffset) {
            return JSValueMakeUndefined(ctx);
        }

        // GLES3 doesn't have glGetBufferSubData. Emulate with glMapBufferRange.
        void* mapped = glMapBufferRange(target, srcByteOffset, static_cast<GLsizeiptr>(byteLen), GL_MAP_READ_BIT);
        if (mapped) {
            std::memcpy(dstPtr, mapped, byteLen);
            glUnmapBuffer(target);
        }
        return JSValueMakeUndefined(ctx);
    }

}  // namespace PrismaUI::WebGL
