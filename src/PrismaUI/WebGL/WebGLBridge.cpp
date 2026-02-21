#include "WebGLBridge.h"

#include "ANGLEContext.h"
#include "PrismaUI/Core.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <JavaScriptCore/JavaScript.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <string>
#include <vector>

namespace PrismaUI::WebGL {

    // =========================================================================
    // Helper: extract ANGLEContext from JSC thisObject's private data
    // =========================================================================
    static ANGLEContext* GetContext(JSObjectRef thisObject) {
        return static_cast<ANGLEContext*>(JSObjectGetPrivate(thisObject));
    }

    // =========================================================================
    // Helper: extract GLuint _id from a WebGL wrapper object (WebGLBuffer, etc.)
    // =========================================================================
    static GLuint GetGLId(JSContextRef ctx, JSValueRef val) {
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
    static JSValueRef MakeGLObject(JSContextRef ctx, const char* className, GLuint id) {
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
    static std::string GetString(JSContextRef ctx, JSValueRef val) {
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

    // =========================================================================
    // WebGL function implementations
    // =========================================================================

    // --- Context/State ---

    static JSValueRef GL_getError(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, static_cast<double>(glGetError()));
    }

    static JSValueRef GL_enable(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glEnable(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_disable(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glDisable(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_viewport(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glViewport(
            static_cast<GLint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_scissor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glScissor(
            static_cast<GLint>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_clearColor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glClearColor(
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_clearDepth(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glClearDepthf(static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_clearStencil(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glClearStencil(static_cast<GLint>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_clear(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glClear(static_cast<GLbitfield>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_colorMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glColorMask(
            JSValueToBoolean(ctx, argv[0]),
            JSValueToBoolean(ctx, argv[1]),
            JSValueToBoolean(ctx, argv[2]),
            JSValueToBoolean(ctx, argv[3]));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_depthFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glDepthFunc(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_depthMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glDepthMask(JSValueToBoolean(ctx, argv[0]));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_depthRange(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glDepthRangef(
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_pixelStorei(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint param = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        // WebGL-specific pixel store params are handled by ANGLE if supported,
        // otherwise we filter them out
        glPixelStorei(pname, param);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_flush(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeUndefined(ctx);
        glFlush();
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_finish(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeUndefined(ctx);
        glFinish();
        return JSValueMakeUndefined(ctx);
    }

    // --- Blend ---

    static JSValueRef GL_blendFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glBlendFunc(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_blendFuncSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glBlendFuncSeparate(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_blendEquation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glBlendEquation(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_blendEquationSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glBlendEquationSeparate(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_blendColor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glBlendColor(
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // --- Culling ---

    static JSValueRef GL_cullFace(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glCullFace(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_frontFace(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glFrontFace(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_lineWidth(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glLineWidth(static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_polygonOffset(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glPolygonOffset(
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // --- Buffers ---

    static JSValueRef GL_createBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint buf = 0;
        glGenBuffers(1, &buf);
        return MakeGLObject(ctx, "WebGLBuffer", buf);
    }

    static JSValueRef GL_deleteBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint buf = GetGLId(ctx, argv[0]);
        if (buf) glDeleteBuffers(1, &buf);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_bindBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint buf = GetGLId(ctx, argv[1]);
        glBindBuffer(target, buf);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_bufferData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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
                    // null data
                    glBufferData(target, 0, nullptr, usage);
                }
            }
        } else {
            glBufferData(target, 0, nullptr, usage);
        }
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_bufferSubData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // --- Shaders ---

    static JSValueRef GL_createShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeNull(ctx);
        GLenum type = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint shader = glCreateShader(type);
        return MakeGLObject(ctx, "WebGLShader", shader);
    }

    static JSValueRef GL_deleteShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint shader = GetGLId(ctx, argv[0]);
        if (shader) glDeleteShader(shader);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_shaderSource(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_compileShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glCompileShader(GetGLId(ctx, argv[0]));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_getShaderParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_getShaderInfoLog(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeString(ctx, JSStringCreateWithUTF8CString(""));
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

    static JSValueRef GL_createProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint prog = glCreateProgram();
        return MakeGLObject(ctx, "WebGLProgram", prog);
    }

    static JSValueRef GL_deleteProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        if (prog) glDeleteProgram(prog);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_attachShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glAttachShader(GetGLId(ctx, argv[0]), GetGLId(ctx, argv[1]));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_linkProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glLinkProgram(GetGLId(ctx, argv[0]));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_useProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glUseProgram(GetGLId(ctx, argv[0]));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_getProgramParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_getProgramInfoLog(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_validateProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                          size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glValidateProgram(GetGLId(ctx, argv[0]));
        return JSValueMakeUndefined(ctx);
    }

    // --- Attributes ---

    static JSValueRef GL_getAttribLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNumber(ctx, -1);
        GLuint prog = GetGLId(ctx, argv[0]);
        std::string name = GetString(ctx, argv[1]);
        GLint loc = glGetAttribLocation(prog, name.c_str());
        return JSValueMakeNumber(ctx, static_cast<double>(loc));
    }

    static JSValueRef GL_enableVertexAttribArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glEnableVertexAttribArray(static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_disableVertexAttribArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glDisableVertexAttribArray(static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_vertexAttribPointer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_bindAttribLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        std::string name = GetString(ctx, argv[2]);
        glBindAttribLocation(prog, index, name.c_str());
        return JSValueMakeUndefined(ctx);
    }

    // --- Uniforms ---

    static JSValueRef GL_getUniformLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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
        static JSValueRef GL_uniform1##suffix(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, \
            size_t argc, const JSValueRef argv[], JSValueRef*) { \
            auto* c = GetContext(thisObject); \
            if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx); \
            GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0])); \
            glFunc(loc, static_cast<castType>(JSValueToNumber(ctx, argv[1], nullptr))); \
            return JSValueMakeUndefined(ctx); \
        }

    UNIFORM_1(f, glUniform1f, GLfloat)
    UNIFORM_1(i, glUniform1i, GLint)

    // Explicit implementations for uniform2f, uniform3f, uniform4f
    static JSValueRef GL_uniform2f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[0]));
        glUniform2f(loc,
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_uniform3f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_uniform4f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // uniformNfv/iv functions — accept typed arrays
    static JSValueRef GL_uniform1fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_uniform2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_uniform3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_uniform4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_uniform1iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // uniformMatrix
    static JSValueRef GL_uniformMatrix3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_uniformMatrix4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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
                glUniformMatrix4fv(loc, static_cast<GLsizei>(byteLen / (16 * sizeof(GLfloat))), transpose, ptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_uniformMatrix2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // --- Textures ---

    static JSValueRef GL_createTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint tex = 0;
        glGenTextures(1, &tex);
        return MakeGLObject(ctx, "WebGLTexture", tex);
    }

    static JSValueRef GL_deleteTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint tex = GetGLId(ctx, argv[0]);
        if (tex) glDeleteTextures(1, &tex);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_bindTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint tex = GetGLId(ctx, argv[1]);
        glBindTexture(target, tex);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_activeTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glActiveTexture(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_texParameteri(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glTexParameteri(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_texParameterf(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glTexParameterf(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_generateMipmap(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                         size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glGenerateMipmap(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_texImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_texSubImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // --- Framebuffers ---

    static JSValueRef GL_createFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                            size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        return MakeGLObject(ctx, "WebGLFramebuffer", fbo);
    }

    static JSValueRef GL_deleteFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint fbo = GetGLId(ctx, argv[0]);
        if (fbo) glDeleteFramebuffers(1, &fbo);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_bindFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                          size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint fbo = GetGLId(ctx, argv[1]);
        glBindFramebuffer(target, fbo);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_framebufferTexture2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    static JSValueRef GL_checkFramebufferStatus(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeNumber(ctx, 0);
        GLenum result = glCheckFramebufferStatus(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeNumber(ctx, static_cast<double>(result));
    }

    static JSValueRef GL_framebufferRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // --- Renderbuffers ---

    static JSValueRef GL_createRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                             size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint rbo = 0;
        glGenRenderbuffers(1, &rbo);
        return MakeGLObject(ctx, "WebGLRenderbuffer", rbo);
    }

    static JSValueRef GL_deleteRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint rbo = GetGLId(ctx, argv[0]);
        if (rbo) glDeleteRenderbuffers(1, &rbo);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_bindRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glBindRenderbuffer(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            GetGLId(ctx, argv[1]));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_renderbufferStorage(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // --- Drawing ---

    static JSValueRef GL_drawArrays(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glDrawArrays(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_drawElements(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glDrawElements(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLsizei>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            reinterpret_cast<const void*>(
                static_cast<intptr_t>(JSValueToNumber(ctx, argv[3], nullptr))));
        return JSValueMakeUndefined(ctx);
    }

    // --- Query functions ---

    static JSValueRef GL_getParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeNull(ctx);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));

        switch (pname) {
            case GL_RENDERER:
            case GL_VENDOR:
            case GL_VERSION:
            case GL_SHADING_LANGUAGE_VERSION: {
                const char* str = reinterpret_cast<const char*>(glGetString(pname));
                if (!str) return JSValueMakeNull(ctx);
                JSStringRef jsStr = JSStringCreateWithUTF8CString(str);
                JSValueRef result = JSValueMakeString(ctx, jsStr);
                JSStringRelease(jsStr);
                return result;
            }
            case GL_MAX_TEXTURE_SIZE:
            case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
            case GL_MAX_RENDERBUFFER_SIZE:
            case GL_MAX_VERTEX_ATTRIBS:
            case GL_MAX_VERTEX_UNIFORM_VECTORS:
            case GL_MAX_VARYING_VECTORS:
            case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
            case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            case GL_MAX_TEXTURE_IMAGE_UNITS:
            case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
            case GL_SUBPIXEL_BITS:
            case GL_RED_BITS:
            case GL_GREEN_BITS:
            case GL_BLUE_BITS:
            case GL_ALPHA_BITS:
            case GL_DEPTH_BITS:
            case GL_STENCIL_BITS:
            case GL_SAMPLE_BUFFERS:
            case GL_SAMPLES: {
                GLint val = 0;
                glGetIntegerv(pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
            case GL_MAX_VIEWPORT_DIMS: {
                GLint dims[2] = {0, 0};
                glGetIntegerv(pname, dims);
                // Return as Int32Array (simplified: return array)
                JSValueRef vals[2] = {
                    JSValueMakeNumber(ctx, dims[0]),
                    JSValueMakeNumber(ctx, dims[1])};
                return JSObjectMakeArray(ctx, 2, vals, nullptr);
            }
            case GL_VIEWPORT:
            case GL_SCISSOR_BOX: {
                GLint box[4] = {};
                glGetIntegerv(pname, box);
                JSValueRef vals[4];
                for (int i = 0; i < 4; i++) vals[i] = JSValueMakeNumber(ctx, box[i]);
                return JSObjectMakeArray(ctx, 4, vals, nullptr);
            }
            case GL_COLOR_CLEAR_VALUE: {
                GLfloat color[4] = {};
                glGetFloatv(pname, color);
                JSValueRef vals[4];
                for (int i = 0; i < 4; i++) vals[i] = JSValueMakeNumber(ctx, color[i]);
                return JSObjectMakeArray(ctx, 4, vals, nullptr);
            }
            case GL_BLEND:
            case GL_CULL_FACE:
            case GL_DEPTH_TEST:
            case GL_DITHER:
            case GL_POLYGON_OFFSET_FILL:
            case GL_SAMPLE_ALPHA_TO_COVERAGE:
            case GL_SAMPLE_COVERAGE:
            case GL_SCISSOR_TEST:
            case GL_STENCIL_TEST: {
                GLboolean val = GL_FALSE;
                glGetBooleanv(pname, &val);
                return JSValueMakeBoolean(ctx, val);
            }
            default: {
                GLint val = 0;
                glGetIntegerv(pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
        }
    }

    static JSValueRef GL_getSupportedExtensions(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                 size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSObjectMakeArray(ctx, 0, nullptr, nullptr);

        // Return common WebGL extensions that ANGLE supports
        const char* exts[] = {
            "OES_element_index_uint",
            "OES_standard_derivatives",
            "OES_texture_float",
            "OES_texture_half_float",
            "WEBGL_depth_texture",
            "WEBGL_lose_context",
        };
        constexpr size_t numExts = sizeof(exts) / sizeof(exts[0]);
        JSValueRef vals[numExts];
        for (size_t i = 0; i < numExts; i++) {
            JSStringRef str = JSStringCreateWithUTF8CString(exts[i]);
            vals[i] = JSValueMakeString(ctx, str);
            JSStringRelease(str);
        }
        return JSObjectMakeArray(ctx, numExts, vals, nullptr);
    }

    static JSValueRef GL_getExtension(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeNull(ctx);
        std::string name = GetString(ctx, argv[0]);

        // Return a truthy object for extensions ANGLE supports
        if (name == "OES_element_index_uint" ||
            name == "OES_standard_derivatives" ||
            name == "OES_texture_float" ||
            name == "OES_texture_half_float" ||
            name == "WEBGL_depth_texture" ||
            name == "EXT_frag_depth" ||
            name == "WEBGL_lose_context" ||
            name == "OES_vertex_array_object" ||
            name == "ANGLE_instanced_arrays") {
            // Return an empty object (truthy) to indicate extension is available
            return JSObjectMake(ctx, nullptr, nullptr);
        }
        return JSValueMakeNull(ctx);
    }

    static JSValueRef GL_isContextLost(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                        size_t, const JSValueRef[], JSValueRef*) {
        return JSValueMakeBoolean(ctx, false);
    }

    static JSValueRef GL_getShaderPrecisionFormat(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLenum shaderType = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum precisionType = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint range[2] = {0, 0};
        GLint precision = 0;
        glGetShaderPrecisionFormat(shaderType, precisionType, range, &precision);
        return MakeGLObject(ctx, "WebGLShaderPrecisionFormat", 0); // simplified
    }

    static JSValueRef GL_getActiveAttrib(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                          size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLchar name[256];
        GLsizei len = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveAttrib(prog, index, sizeof(name), &len, &size, &type, name);
        // Return WebGLActiveInfo-like object
        JSObjectRef obj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef sizeKey = JSStringCreateWithUTF8CString("size");
        JSStringRef typeKey = JSStringCreateWithUTF8CString("type");
        JSStringRef nameKey = JSStringCreateWithUTF8CString("name");
        JSObjectSetProperty(ctx, obj, sizeKey, JSValueMakeNumber(ctx, size), 0, nullptr);
        JSObjectSetProperty(ctx, obj, typeKey, JSValueMakeNumber(ctx, type), 0, nullptr);
        JSStringRef nameVal = JSStringCreateWithUTF8CString(name);
        JSObjectSetProperty(ctx, obj, nameKey, JSValueMakeString(ctx, nameVal), 0, nullptr);
        JSStringRelease(sizeKey); JSStringRelease(typeKey); JSStringRelease(nameKey); JSStringRelease(nameVal);
        return obj;
    }

    static JSValueRef GL_getActiveUniform(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLchar name[256];
        GLsizei len = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveUniform(prog, index, sizeof(name), &len, &size, &type, name);
        JSObjectRef obj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef sizeKey = JSStringCreateWithUTF8CString("size");
        JSStringRef typeKey = JSStringCreateWithUTF8CString("type");
        JSStringRef nameKey = JSStringCreateWithUTF8CString("name");
        JSObjectSetProperty(ctx, obj, sizeKey, JSValueMakeNumber(ctx, size), 0, nullptr);
        JSObjectSetProperty(ctx, obj, typeKey, JSValueMakeNumber(ctx, type), 0, nullptr);
        JSStringRef nameVal = JSStringCreateWithUTF8CString(name);
        JSObjectSetProperty(ctx, obj, nameKey, JSValueMakeString(ctx, nameVal), 0, nullptr);
        JSStringRelease(sizeKey); JSStringRelease(typeKey); JSStringRelease(nameKey); JSStringRelease(nameVal);
        return obj;
    }

    // Stencil operations
    static JSValueRef GL_stencilFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glStencilFunc(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_stencilOp(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glStencilOp(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef GL_stencilMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glStencilMask(static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // JSClass definition for WebGLRenderingContext
    // =========================================================================

    static JSStaticFunction kWebGLFunctions[] = {
        // Context/State
        {"getError",                    GL_getError,                    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"enable",                      GL_enable,                      kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"disable",                     GL_disable,                     kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"viewport",                    GL_viewport,                    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"scissor",                     GL_scissor,                     kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"clearColor",                  GL_clearColor,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"clearDepth",                  GL_clearDepth,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"clearStencil",                GL_clearStencil,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"clear",                       GL_clear,                       kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"colorMask",                   GL_colorMask,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"depthFunc",                   GL_depthFunc,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"depthMask",                   GL_depthMask,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"depthRange",                  GL_depthRange,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"pixelStorei",                 GL_pixelStorei,                 kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"flush",                       GL_flush,                       kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"finish",                      GL_finish,                      kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getParameter",                GL_getParameter,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getSupportedExtensions",      GL_getSupportedExtensions,      kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getExtension",                GL_getExtension,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"isContextLost",               GL_isContextLost,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getShaderPrecisionFormat",    GL_getShaderPrecisionFormat,    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Blend
        {"blendFunc",                   GL_blendFunc,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"blendFuncSeparate",           GL_blendFuncSeparate,           kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"blendEquation",               GL_blendEquation,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"blendEquationSeparate",       GL_blendEquationSeparate,       kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"blendColor",                  GL_blendColor,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Culling
        {"cullFace",                    GL_cullFace,                    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"frontFace",                   GL_frontFace,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"lineWidth",                   GL_lineWidth,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"polygonOffset",               GL_polygonOffset,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Buffers
        {"createBuffer",                GL_createBuffer,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"deleteBuffer",                GL_deleteBuffer,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"bindBuffer",                  GL_bindBuffer,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"bufferData",                  GL_bufferData,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"bufferSubData",               GL_bufferSubData,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Shaders
        {"createShader",                GL_createShader,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"deleteShader",                GL_deleteShader,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"shaderSource",                GL_shaderSource,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"compileShader",               GL_compileShader,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getShaderParameter",          GL_getShaderParameter,          kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getShaderInfoLog",            GL_getShaderInfoLog,            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"createProgram",               GL_createProgram,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"deleteProgram",               GL_deleteProgram,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"attachShader",                GL_attachShader,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"linkProgram",                 GL_linkProgram,                 kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"useProgram",                  GL_useProgram,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getProgramParameter",         GL_getProgramParameter,         kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getProgramInfoLog",           GL_getProgramInfoLog,           kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"validateProgram",             GL_validateProgram,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"bindAttribLocation",          GL_bindAttribLocation,          kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getActiveAttrib",             GL_getActiveAttrib,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getActiveUniform",            GL_getActiveUniform,            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Attributes
        {"getAttribLocation",           GL_getAttribLocation,           kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"enableVertexAttribArray",     GL_enableVertexAttribArray,     kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"disableVertexAttribArray",    GL_disableVertexAttribArray,    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"vertexAttribPointer",         GL_vertexAttribPointer,         kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Uniforms
        {"getUniformLocation",          GL_getUniformLocation,          kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform1f",                   GL_uniform1f,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform1i",                   GL_uniform1i,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform2f",                   GL_uniform2f,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform3f",                   GL_uniform3f,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform4f",                   GL_uniform4f,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform1fv",                  GL_uniform1fv,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform2fv",                  GL_uniform2fv,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform3fv",                  GL_uniform3fv,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform4fv",                  GL_uniform4fv,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform1iv",                  GL_uniform1iv,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniformMatrix2fv",            GL_uniformMatrix2fv,            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniformMatrix3fv",            GL_uniformMatrix3fv,            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniformMatrix4fv",            GL_uniformMatrix4fv,            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Textures
        {"createTexture",               GL_createTexture,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"deleteTexture",               GL_deleteTexture,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"bindTexture",                 GL_bindTexture,                 kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"activeTexture",               GL_activeTexture,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"texParameteri",               GL_texParameteri,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"texParameterf",               GL_texParameterf,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"generateMipmap",              GL_generateMipmap,              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"texImage2D",                  GL_texImage2D,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"texSubImage2D",               GL_texSubImage2D,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Framebuffers
        {"createFramebuffer",           GL_createFramebuffer,           kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"deleteFramebuffer",           GL_deleteFramebuffer,           kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"bindFramebuffer",             GL_bindFramebuffer,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"framebufferTexture2D",        GL_framebufferTexture2D,        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"checkFramebufferStatus",      GL_checkFramebufferStatus,      kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"framebufferRenderbuffer",     GL_framebufferRenderbuffer,     kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Renderbuffers
        {"createRenderbuffer",          GL_createRenderbuffer,          kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"deleteRenderbuffer",          GL_deleteRenderbuffer,          kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"bindRenderbuffer",            GL_bindRenderbuffer,            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"renderbufferStorage",         GL_renderbufferStorage,         kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Drawing
        {"drawArrays",                  GL_drawArrays,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"drawElements",                GL_drawElements,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Stencil
        {"stencilFunc",                 GL_stencilFunc,                 kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"stencilOp",                   GL_stencilOp,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"stencilMask",                 GL_stencilMask,                 kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Sentinel
        {nullptr, nullptr, 0}
    };

    static JSClassRef g_WebGLContextClass = nullptr;

    JSClassRef GetWebGLContextClass() {
        if (!g_WebGLContextClass) {
            JSClassDefinition classDef{};  // Zero-init (avoids kJSClassDefinitionEmpty data import that breaks /DELAYLOAD)
            classDef.className = "WebGLRenderingContext";
            classDef.staticFunctions = kWebGLFunctions;
            g_WebGLContextClass = JSClassCreate(&classDef);
        }
        return g_WebGLContextClass;
    }

    // =========================================================================
    // Native function: __prismaCreateWebGLContext(width, height)
    // Called from the JS shim when canvas.getContext('webgl') is invoked.
    // =========================================================================
    static JSValueRef JS_CreateWebGLContext(JSContextRef ctx, JSObjectRef /*function*/,
                                            JSObjectRef /*thisObject*/, size_t argc,
                                            const JSValueRef argv[], JSValueRef* exc) {
        if (argc < 2) {
            spdlog::error("[WebGL] __prismaCreateWebGLContext called with insufficient args");
            return JSValueMakeNull(ctx);
        }

        uint32_t width = static_cast<uint32_t>(JSValueToNumber(ctx, argv[0], exc));
        uint32_t height = static_cast<uint32_t>(JSValueToNumber(ctx, argv[1], exc));

        if (width == 0 || height == 0) {
            width = width ? width : 300;
            height = height ? height : 150;
        }

        // Get the D3D device from Core
        ID3D11Device* device = PrismaUI::Core::d3dDevice;

        if (!device) {
            spdlog::error("[WebGL] D3D11 device not available");
            return JSValueMakeNull(ctx);
        }

        ANGLEContext* angleCtx = CreateWebGLContext(width, height, device);
        if (!angleCtx) {
            spdlog::error("[WebGL] Failed to create ANGLE context");
            return JSValueMakeNull(ctx);
        }

        // Create a JSC object of the WebGLRenderingContext class with angleCtx as private data
        JSClassRef webglClass = GetWebGLContextClass();
        JSObjectRef contextObj = JSObjectMake(ctx, webglClass, angleCtx);

        spdlog::info("[WebGL] WebGL context created and bound to JS: {}x{}", width, height);
        return contextObj;
    }

    // =========================================================================
    // InjectWebGLBindings: called from OnWindowObjectReady
    // =========================================================================
    void InjectWebGLBindings(JSContextRef jsCtx, uint64_t /*viewId*/) {
        JSObjectRef globalObj = JSContextGetGlobalObject(jsCtx);

        // Bind __prismaCreateWebGLContext as a global function
        JSStringRef funcName = JSStringCreateWithUTF8CString("__prismaCreateWebGLContext");
        JSObjectRef funcObj = JSObjectMakeFunctionWithCallback(jsCtx, funcName, JS_CreateWebGLContext);
        JSObjectSetProperty(jsCtx, globalObj, funcName, funcObj, kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(funcName);

        spdlog::info("[WebGL] WebGL bindings injected into JS context");
    }

}  // namespace PrismaUI::WebGL
