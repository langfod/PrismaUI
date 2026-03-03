#include "WebGLBridgeInternal.h"

namespace PrismaUI::WebGL {

    // =========================================================================
    // Helper: extract numeric array from TypedArray or plain JS Array
    // =========================================================================

    template <typename T>
    size_t ExtractNumericArray(JSContextRef ctx, JSValueRef val,
                               T* out, size_t maxCount, const T** directPtr) {
        *directPtr = nullptr;
        if (!JSValueIsObject(ctx, val)) return 0;
        JSObjectRef obj = JSValueToObject(ctx, val, nullptr);
        if (!obj) return 0;

        // Try TypedArray fast path first
        JSTypedArrayType arrType = JSValueGetTypedArrayType(ctx, val, nullptr);
        if (arrType != kJSTypedArrayTypeNone) {
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, obj, nullptr);
            auto* ptr = static_cast<const T*>(JSObjectGetTypedArrayBytesPtr(ctx, obj, nullptr));
            if (ptr && byteLen > 0) {
                *directPtr = ptr;
                return byteLen / sizeof(T);
            }
            return 0;
        }

        // Slow path: plain JS Array (or array-like object with .length)
        JSStringRef lengthKey = JSStringCreateWithUTF8CString("length");
        JSValueRef lengthVal = JSObjectGetProperty(ctx, obj, lengthKey, nullptr);
        JSStringRelease(lengthKey);
        if (JSValueIsUndefined(ctx, lengthVal)) return 0;

        size_t count = static_cast<size_t>(JSValueToNumber(ctx, lengthVal, nullptr));
        if (count > maxCount) count = maxCount;
        for (size_t i = 0; i < count; i++) {
            JSValueRef elem = JSObjectGetPropertyAtIndex(ctx, obj, static_cast<unsigned>(i), nullptr);
            out[i] = static_cast<T>(JSValueToNumber(ctx, elem, nullptr));
        }
        return count;
    }

    // Explicit instantiations
    template size_t ExtractNumericArray<GLfloat>(JSContextRef, JSValueRef, GLfloat*, size_t, const GLfloat**);
    template size_t ExtractNumericArray<GLint>(JSContextRef, JSValueRef, GLint*, size_t, const GLint**);
    template size_t ExtractNumericArray<GLuint>(JSContextRef, JSValueRef, GLuint*, size_t, const GLuint**);

    // Max elements for stack-allocated temp buffers (covers uniformMatrix4fv with up to 16 matrices)
    static constexpr size_t kMaxTempFloats = 256;
    static constexpr size_t kMaxTempInts = 256;

    // =========================================================================
    // Uniforms
    // =========================================================================

    JSValueRef GL_getUniformLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        std::string name = GetString(ctx, argv[1]);
        GLint loc = glGetUniformLocation(prog, name.c_str());
        if (loc < 0) return JSValueMakeNull(ctx);
        return MakeGLObject(ctx, "WebGLUniformLocation", static_cast<GLuint>(loc));
    }

    #define UNIFORM_1(suffix, glFunc, castType) \
        JSValueRef GL_uniform1##suffix(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, \
            size_t argc, const JSValueRef argv[], JSValueRef*) { \
            auto* c = GetContext(thisObject); \
            if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx); \
            GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0])); \
            glFunc(loc, static_cast<castType>(JSValueToNumber(ctx, argv[1], nullptr))); \
            return JSValueMakeUndefined(ctx); \
        }

    UNIFORM_1(f, glUniform1f, GLfloat)
    UNIFORM_1(i, glUniform1i, GLint)

    #undef UNIFORM_1

    // Explicit implementations for uniform2f, uniform3f, uniform4f
    JSValueRef GL_uniform2f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform2f(loc,
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform3f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform3f(loc,
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform4f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform4f(loc,
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // Integer scalar uniforms
    JSValueRef GL_uniform2i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform2i(loc,
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform3i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform3i(loc,
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform4i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform4i(loc,
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // uniformNfv/iv functions — accept typed arrays or plain JS Arrays
    JSValueRef GL_uniform1fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLfloat tmp[kMaxTempFloats]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempFloats, &ptr);
        if (n > 0) glUniform1fv(loc, static_cast<GLsizei>(n), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLfloat tmp[kMaxTempFloats]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempFloats, &ptr);
        if (n >= 2) glUniform2fv(loc, static_cast<GLsizei>(n / 2), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLfloat tmp[kMaxTempFloats]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempFloats, &ptr);
        if (n >= 3) glUniform3fv(loc, static_cast<GLsizei>(n / 3), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLfloat tmp[kMaxTempFloats]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempFloats, &ptr);
        if (n >= 4) glUniform4fv(loc, static_cast<GLsizei>(n / 4), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform1iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLint tmp[kMaxTempInts]; const GLint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempInts, &ptr);
        if (n > 0) glUniform1iv(loc, static_cast<GLsizei>(n), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform2iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLint tmp[kMaxTempInts]; const GLint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempInts, &ptr);
        if (n >= 2) glUniform2iv(loc, static_cast<GLsizei>(n / 2), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform3iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLint tmp[kMaxTempInts]; const GLint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempInts, &ptr);
        if (n >= 3) glUniform3iv(loc, static_cast<GLsizei>(n / 3), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform4iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLint tmp[kMaxTempInts]; const GLint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempInts, &ptr);
        if (n >= 4) glUniform4iv(loc, static_cast<GLsizei>(n / 4), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    // uniformMatrix
    JSValueRef GL_uniformMatrix2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLboolean transpose = JSValueToBoolean(ctx, argv[1]);
        GLfloat tmp[kMaxTempFloats]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[2], tmp, kMaxTempFloats, &ptr);
        if (n >= 4) glUniformMatrix2fv(loc, static_cast<GLsizei>(n / 4), transpose, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniformMatrix3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLboolean transpose = JSValueToBoolean(ctx, argv[1]);
        GLfloat tmp[kMaxTempFloats]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[2], tmp, kMaxTempFloats, &ptr);
        if (n >= 9) glUniformMatrix3fv(loc, static_cast<GLsizei>(n / 9), transpose, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniformMatrix4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLboolean transpose = JSValueToBoolean(ctx, argv[1]);
        GLfloat tmp[kMaxTempFloats]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[2], tmp, kMaxTempFloats, &ptr);
        if (n >= 16) glUniformMatrix4fv(loc, static_cast<GLsizei>(n / 16), transpose, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Vertex Attrib Constants
    // =========================================================================

    JSValueRef GL_vertexAttrib1f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glVertexAttrib1f(
            static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib2f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glVertexAttrib2f(
            static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib3f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glVertexAttrib3f(
            static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib4f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        glVertexAttrib4f(
            static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib1fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLfloat tmp[4]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, 4, &ptr);
        if (n >= 1) glVertexAttrib1fv(index, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLfloat tmp[4]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, 4, &ptr);
        if (n >= 2) glVertexAttrib2fv(index, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLfloat tmp[4]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, 4, &ptr);
        if (n >= 3) glVertexAttrib3fv(index, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLfloat tmp[4]; const GLfloat* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, 4, &ptr);
        if (n >= 4) glVertexAttrib4fv(index, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Uint Uniforms (WebGL2)
    // =========================================================================

    JSValueRef GL_uniform1ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform1ui(loc, static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform2ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform2ui(loc,
            static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform3ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform3ui(loc,
            static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform4ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform4ui(loc,
            static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform1uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLuint tmp[kMaxTempInts]; const GLuint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempInts, &ptr);
        if (n > 0) glUniform1uiv(loc, static_cast<GLsizei>(n), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform2uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLuint tmp[kMaxTempInts]; const GLuint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempInts, &ptr);
        if (n >= 2) glUniform2uiv(loc, static_cast<GLsizei>(n / 2), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform3uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLuint tmp[kMaxTempInts]; const GLuint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempInts, &ptr);
        if (n >= 3) glUniform3uiv(loc, static_cast<GLsizei>(n / 3), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform4uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLuint tmp[kMaxTempInts]; const GLuint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, kMaxTempInts, &ptr);
        if (n >= 4) glUniform4uiv(loc, static_cast<GLsizei>(n / 4), ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Non-square Matrix Uniforms (WebGL2)
    // =========================================================================

    #define UNIFORM_MAT_NONSQUARE(name, glFunc, divisor) \
        JSValueRef GL_##name(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, \
            size_t argc, const JSValueRef argv[], JSValueRef*) { \
            auto* c = GetContext(thisObject); \
            if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx); \
            GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0])); \
            GLboolean transpose = JSValueToBoolean(ctx, argv[1]); \
            GLfloat tmp[kMaxTempFloats]; const GLfloat* ptr; \
            size_t n = ExtractNumericArray(ctx, argv[2], tmp, kMaxTempFloats, &ptr); \
            if (n >= divisor) \
                glFunc(loc, static_cast<GLsizei>(n / divisor), transpose, ptr ? ptr : tmp); \
            return JSValueMakeUndefined(ctx); \
        }

    UNIFORM_MAT_NONSQUARE(uniformMatrix2x3fv, glUniformMatrix2x3fv, 6)
    UNIFORM_MAT_NONSQUARE(uniformMatrix3x2fv, glUniformMatrix3x2fv, 6)
    UNIFORM_MAT_NONSQUARE(uniformMatrix2x4fv, glUniformMatrix2x4fv, 8)
    UNIFORM_MAT_NONSQUARE(uniformMatrix4x2fv, glUniformMatrix4x2fv, 8)
    UNIFORM_MAT_NONSQUARE(uniformMatrix3x4fv, glUniformMatrix3x4fv, 12)
    UNIFORM_MAT_NONSQUARE(uniformMatrix4x3fv, glUniformMatrix4x3fv, 12)

    #undef UNIFORM_MAT_NONSQUARE

    // =========================================================================
    // Integer Vertex Attribs (WebGL2)
    // =========================================================================

    JSValueRef GL_vertexAttribIPointer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint size = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr));
        GLsizei stride = static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr));
        auto offset = static_cast<intptr_t>(JSValueToNumber(ctx, argv[4], nullptr));
        glVertexAttribIPointer(index, size, type, stride, reinterpret_cast<const void*>(offset));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttribI4i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        glVertexAttribI4i(
            static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttribI4ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        glVertexAttribI4ui(
            static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[3], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[4], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttribI4iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint tmp[4]; const GLint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, 4, &ptr);
        if (n >= 4) glVertexAttribI4iv(index, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttribI4uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint tmp[4]; const GLuint* ptr;
        size_t n = ExtractNumericArray(ctx, argv[1], tmp, 4, &ptr);
        if (n >= 4) glVertexAttribI4uiv(index, ptr ? ptr : tmp);
        return JSValueMakeUndefined(ctx);
    }

}  // namespace PrismaUI::WebGL
