#include "WebGLBridgeInternal.h"

namespace PrismaUI::WebGL {

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

    // uniformNfv/iv functions — accept typed arrays
    JSValueRef GL_uniform1fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniform1fv(loc, static_cast<GLsizei>(byteLen / sizeof(GLfloat)), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniform2fv(loc, static_cast<GLsizei>(byteLen / (2 * sizeof(GLfloat))), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniform3fv(loc, static_cast<GLsizei>(byteLen / (3 * sizeof(GLfloat))), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniform4fv(loc, static_cast<GLsizei>(byteLen / (4 * sizeof(GLfloat))), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform1iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLint*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniform1iv(loc, static_cast<GLsizei>(byteLen / sizeof(GLint)), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform2iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLint*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniform2iv(loc, static_cast<GLsizei>(byteLen / (2 * sizeof(GLint))), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform3iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLint*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniform3iv(loc, static_cast<GLsizei>(byteLen / (3 * sizeof(GLint))), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniform4iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLint*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniform4iv(loc, static_cast<GLsizei>(byteLen / (4 * sizeof(GLint))), ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    // uniformMatrix
    JSValueRef GL_uniformMatrix2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLboolean transpose = JSValueToBoolean(ctx, argv[1]);
        if (JSValueIsObject(ctx, argv[2])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[2], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniformMatrix2fv(loc, static_cast<GLsizei>(byteLen / (4 * sizeof(GLfloat))), transpose, ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniformMatrix3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLboolean transpose = JSValueToBoolean(ctx, argv[1]);
        if (JSValueIsObject(ctx, argv[2])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[2], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                glUniformMatrix3fv(loc, static_cast<GLsizei>(byteLen / (9 * sizeof(GLfloat))), transpose, ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniformMatrix4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        GLboolean transpose = JSValueToBoolean(ctx, argv[1]);
        if (JSValueIsObject(ctx, argv[2])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[2], nullptr);
            size_t byteLen = JSObjectGetTypedArrayByteLength(ctx, arr, nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr && byteLen > 0) {
                GLsizei count = static_cast<GLsizei>(byteLen / (16 * sizeof(GLfloat)));
                glUniformMatrix4fv(loc, count, transpose, ptr);
            }
        }
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
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr) glVertexAttrib1fv(index, ptr);
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr) glVertexAttrib2fv(index, ptr);
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr) glVertexAttrib3fv(index, ptr);
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttrib4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        if (JSValueIsObject(ctx, argv[1])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
            auto* ptr = static_cast<const GLfloat*>(JSObjectGetTypedArrayBytesPtr(ctx, arr, nullptr));
            if (ptr) glVertexAttrib4fv(index, ptr);
        }
        return JSValueMakeUndefined(ctx);
    }

}  // namespace PrismaUI::WebGL
