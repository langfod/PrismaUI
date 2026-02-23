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
                void* ptr = JSObjectGetTypedArrayBytesPtr(ctx, dataObj, nullptr);
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
                void* ptr = JSObjectGetTypedArrayBytesPtr(ctx, dataObj, nullptr);
                glBufferSubData(target, offset, static_cast<GLsizeiptr>(byteLen), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

}  // namespace PrismaUI::WebGL
