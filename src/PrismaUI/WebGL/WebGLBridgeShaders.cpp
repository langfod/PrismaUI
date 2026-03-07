#include "WebGLBridgeInternal.h"

#include <vector>

namespace PrismaUI::WebGL {

    // =========================================================================
    // Shaders
    // =========================================================================

    JSValueRef GL_createShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeNull(ctx);
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint shader = glCreateShader(type);
        return MakeGLObject(ctx, "WebGLShader", shader);
    }

    JSValueRef GL_deleteShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint shader = GetGLId(ctx, argv[0]);
        if (shader) glDeleteShader(shader);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_shaderSource(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint shader = GetGLId(ctx, argv[0]);
        std::string source = GetString(ctx, argv[1]);
        const char* src = source.c_str();
        GLint len = static_cast<GLint>(source.size());
        glShaderSource(shader, 1, &src, &len);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_compileShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint shader = GetGLId(ctx, argv[0]);
        glCompileShader(shader);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_getShaderParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint shader = GetGLId(ctx, argv[0]);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint val = 0;
        glGetShaderiv(shader, pname, &val);
        if (pname == GL_COMPILE_STATUS || pname == GL_DELETE_STATUS) {
            return JSValueMakeBoolean(ctx, val != 0);
        }
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

    JSValueRef GL_getShaderInfoLog(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) {
            JSStringRef s = JSStringCreateWithUTF8CString("");
            JSValueRef r = JSValueMakeString(ctx, s);
            JSStringRelease(s);
            return r;
        }
        GLuint shader = GetGLId(ctx, argv[0]);
        GLint logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen <= 0) {
            JSStringRef empty = JSStringCreateWithUTF8CString("");
            JSValueRef result = JSValueMakeString(ctx, empty);
            JSStringRelease(empty);
            return result;
        }
        std::vector<char> buf(logLen);
        glGetShaderInfoLog(shader, logLen, nullptr, buf.data());
        JSStringRef str = JSStringCreateWithUTF8CString(buf.data());
        JSValueRef result = JSValueMakeString(ctx, str);
        JSStringRelease(str);
        return result;
    }

    // =========================================================================
    // Programs
    // =========================================================================

    JSValueRef GL_createProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint prog = glCreateProgram();
        return MakeGLObject(ctx, "WebGLProgram", prog);
    }

    JSValueRef GL_deleteProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        if (prog) glDeleteProgram(prog);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_attachShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glAttachShader(GetGLId(ctx, argv[0]), GetGLId(ctx, argv[1]));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_detachShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glDetachShader(GetGLId(ctx, argv[0]), GetGLId(ctx, argv[1]));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_linkProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        glLinkProgram(prog);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_useProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glUseProgram(GetGLId(ctx, argv[0]));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_getProgramParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint val = 0;
        glGetProgramiv(prog, pname, &val);
        if (pname == GL_LINK_STATUS || pname == GL_VALIDATE_STATUS || pname == GL_DELETE_STATUS) {
            return JSValueMakeBoolean(ctx, val != 0);
        }
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

    JSValueRef GL_getProgramInfoLog(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) {
            JSStringRef empty = JSStringCreateWithUTF8CString("");
            JSValueRef result = JSValueMakeString(ctx, empty);
            JSStringRelease(empty);
            return result;
        }
        GLuint prog = GetGLId(ctx, argv[0]);
        GLint logLen = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen <= 0) {
            JSStringRef empty = JSStringCreateWithUTF8CString("");
            JSValueRef result = JSValueMakeString(ctx, empty);
            JSStringRelease(empty);
            return result;
        }
        std::vector<char> buf(logLen);
        glGetProgramInfoLog(prog, logLen, nullptr, buf.data());
        JSStringRef str = JSStringCreateWithUTF8CString(buf.data());
        JSValueRef result = JSValueMakeString(ctx, str);
        JSStringRelease(str);
        return result;
    }

    JSValueRef GL_validateProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glValidateProgram(GetGLId(ctx, argv[0]));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Attributes
    // =========================================================================

    JSValueRef GL_getAttribLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNumber(ctx, -1);
        GLuint prog = GetGLId(ctx, argv[0]);
        std::string name = GetString(ctx, argv[1]);
        GLint loc = glGetAttribLocation(prog, name.c_str());
        return JSValueMakeNumber(ctx, static_cast<double>(loc));
    }

    JSValueRef GL_enableVertexAttribArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                          size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glEnableVertexAttribArray(static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_disableVertexAttribArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glDisableVertexAttribArray(static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_vertexAttribPointer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 6) return JSValueMakeUndefined(ctx);
        glVertexAttribPointer(
            static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)),   // index
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),    // size
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),   // type
            JSValueToBoolean(ctx, argv[3]),                                 // normalized
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[4], nullptr)),  // stride
            reinterpret_cast<const void*>(                                  // offset
                static_cast<intptr_t>(JSValueToNumber(ctx, argv[5], nullptr))));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindAttribLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        std::string name = GetString(ctx, argv[2]);
        glBindAttribLocation(prog, index, name.c_str());
        return JSValueMakeUndefined(ctx);
    }

}  // namespace PrismaUI::WebGL
