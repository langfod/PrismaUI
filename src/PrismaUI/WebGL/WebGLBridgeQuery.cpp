#include "WebGLBridgeInternal.h"

#include <vector>

namespace PrismaUI::WebGL {

    // =========================================================================
    // Query / Introspection
    // =========================================================================

    JSValueRef GL_getParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_getSupportedExtensions(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_getExtension(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_isContextLost(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                size_t, const JSValueRef[], JSValueRef*) {
        return JSValueMakeBoolean(ctx, false);
    }

    JSValueRef GL_getShaderPrecisionFormat(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_getActiveAttrib(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_getActiveUniform(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // =========================================================================
    // Boolean is* queries
    // =========================================================================

    JSValueRef GL_isEnabled(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, glIsEnabled(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr))));
    }

    JSValueRef GL_isBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, glIsBuffer(GetGLId(ctx, argv[0])));
    }

    JSValueRef GL_isFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, glIsFramebuffer(GetGLId(ctx, argv[0])));
    }

    JSValueRef GL_isRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, glIsRenderbuffer(GetGLId(ctx, argv[0])));
    }

    JSValueRef GL_isTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, glIsTexture(GetGLId(ctx, argv[0])));
    }

    JSValueRef GL_isProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, glIsProgram(GetGLId(ctx, argv[0])));
    }

    JSValueRef GL_isShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, glIsShader(GetGLId(ctx, argv[0])));
    }

    // =========================================================================
    // Simple state queries
    // =========================================================================

    JSValueRef GL_getBufferParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint val = 0;
        glGetBufferParameteriv(target, pname, &val);
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

    JSValueRef GL_getRenderbufferParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint val = 0;
        glGetRenderbufferParameteriv(target, pname, &val);
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

    JSValueRef GL_getTexParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint val = 0;
        glGetTexParameteriv(target, pname, &val);
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

    JSValueRef GL_getFramebufferAttachmentParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeNull(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum attachment = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr));
        GLint val = 0;
        glGetFramebufferAttachmentParameteriv(target, attachment, pname, &val);
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

    // =========================================================================
    // String/object-returning queries
    // =========================================================================

    JSValueRef GL_getContextAttributes(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        JSObjectRef obj = JSObjectMake(ctx, nullptr, nullptr);
        auto setProp = [&](const char* key, JSValueRef val) {
            JSStringRef k = JSStringCreateWithUTF8CString(key);
            JSObjectSetProperty(ctx, obj, k, val, 0, nullptr);
            JSStringRelease(k);
        };
        setProp("alpha", JSValueMakeBoolean(ctx, true));
        setProp("depth", JSValueMakeBoolean(ctx, true));
        setProp("stencil", JSValueMakeBoolean(ctx, true));
        setProp("antialias", JSValueMakeBoolean(ctx, false));
        setProp("premultipliedAlpha", JSValueMakeBoolean(ctx, true));
        setProp("preserveDrawingBuffer", JSValueMakeBoolean(ctx, false));
        setProp("failIfMajorPerformanceCaveat", JSValueMakeBoolean(ctx, false));
        return obj;
    }

    JSValueRef GL_getShaderSource(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) {
            JSStringRef empty = JSStringCreateWithUTF8CString("");
            JSValueRef result = JSValueMakeString(ctx, empty);
            JSStringRelease(empty);
            return result;
        }
        GLuint shader = GetGLId(ctx, argv[0]);
        GLint srcLen = 0;
        glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &srcLen);
        if (srcLen <= 0) {
            JSStringRef empty = JSStringCreateWithUTF8CString("");
            JSValueRef result = JSValueMakeString(ctx, empty);
            JSStringRelease(empty);
            return result;
        }
        std::vector<char> buf(srcLen);
        glGetShaderSource(shader, srcLen, nullptr, buf.data());
        JSStringRef str = JSStringCreateWithUTF8CString(buf.data());
        JSValueRef result = JSValueMakeString(ctx, str);
        JSStringRelease(str);
        return result;
    }

    JSValueRef GL_getAttachedShaders(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSObjectMakeArray(ctx, 0, nullptr, nullptr);
        GLuint prog = GetGLId(ctx, argv[0]);
        GLint count = 0;
        glGetProgramiv(prog, GL_ATTACHED_SHADERS, &count);
        if (count <= 0) return JSObjectMakeArray(ctx, 0, nullptr, nullptr);
        std::vector<GLuint> shaders(count);
        glGetAttachedShaders(prog, count, nullptr, shaders.data());
        std::vector<JSValueRef> vals(count);
        for (GLint i = 0; i < count; i++) {
            vals[i] = MakeGLObject(ctx, "WebGLShader", shaders[i]);
        }
        return JSObjectMakeArray(ctx, static_cast<size_t>(count), vals.data(), nullptr);
    }

    JSValueRef GL_getUniform(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        GLint loc = static_cast<GLint>(GetGLId(ctx, argv[1]));

        // Find the uniform's type by iterating active uniforms
        GLint numUniforms = 0;
        glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &numUniforms);
        GLenum uniformType = 0;
        for (GLint i = 0; i < numUniforms; i++) {
            GLchar name[256];
            GLsizei len = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(prog, i, sizeof(name), &len, &size, &type, name);
            GLint uLoc = glGetUniformLocation(prog, name);
            if (uLoc == loc) {
                uniformType = type;
                break;
            }
        }
        if (uniformType == 0) return JSValueMakeNull(ctx);

        switch (uniformType) {
            case GL_FLOAT: {
                GLfloat val = 0;
                glGetUniformfv(prog, loc, &val);
                return JSValueMakeNumber(ctx, val);
            }
            case GL_FLOAT_VEC2: {
                GLfloat val[2];
                glGetUniformfv(prog, loc, val);
                JSValueRef vals[2] = { JSValueMakeNumber(ctx, val[0]), JSValueMakeNumber(ctx, val[1]) };
                return JSObjectMakeArray(ctx, 2, vals, nullptr);
            }
            case GL_FLOAT_VEC3: {
                GLfloat val[3];
                glGetUniformfv(prog, loc, val);
                JSValueRef vals[3];
                for (int i = 0; i < 3; i++) vals[i] = JSValueMakeNumber(ctx, val[i]);
                return JSObjectMakeArray(ctx, 3, vals, nullptr);
            }
            case GL_FLOAT_VEC4: {
                GLfloat val[4];
                glGetUniformfv(prog, loc, val);
                JSValueRef vals[4];
                for (int i = 0; i < 4; i++) vals[i] = JSValueMakeNumber(ctx, val[i]);
                return JSObjectMakeArray(ctx, 4, vals, nullptr);
            }
            case GL_INT:
            case GL_SAMPLER_2D:
            case GL_SAMPLER_CUBE: {
                GLint val = 0;
                glGetUniformiv(prog, loc, &val);
                return JSValueMakeNumber(ctx, val);
            }
            case GL_INT_VEC2: {
                GLint val[2];
                glGetUniformiv(prog, loc, val);
                JSValueRef vals[2] = { JSValueMakeNumber(ctx, val[0]), JSValueMakeNumber(ctx, val[1]) };
                return JSObjectMakeArray(ctx, 2, vals, nullptr);
            }
            case GL_INT_VEC3: {
                GLint val[3];
                glGetUniformiv(prog, loc, val);
                JSValueRef vals[3];
                for (int i = 0; i < 3; i++) vals[i] = JSValueMakeNumber(ctx, val[i]);
                return JSObjectMakeArray(ctx, 3, vals, nullptr);
            }
            case GL_INT_VEC4: {
                GLint val[4];
                glGetUniformiv(prog, loc, val);
                JSValueRef vals[4];
                for (int i = 0; i < 4; i++) vals[i] = JSValueMakeNumber(ctx, val[i]);
                return JSObjectMakeArray(ctx, 4, vals, nullptr);
            }
            case GL_BOOL: {
                GLint val = 0;
                glGetUniformiv(prog, loc, &val);
                return JSValueMakeBoolean(ctx, val != 0);
            }
            case GL_BOOL_VEC2: {
                GLint val[2];
                glGetUniformiv(prog, loc, val);
                JSValueRef vals[2] = { JSValueMakeBoolean(ctx, val[0] != 0), JSValueMakeBoolean(ctx, val[1] != 0) };
                return JSObjectMakeArray(ctx, 2, vals, nullptr);
            }
            case GL_BOOL_VEC3: {
                GLint val[3];
                glGetUniformiv(prog, loc, val);
                JSValueRef vals[3];
                for (int i = 0; i < 3; i++) vals[i] = JSValueMakeBoolean(ctx, val[i] != 0);
                return JSObjectMakeArray(ctx, 3, vals, nullptr);
            }
            case GL_BOOL_VEC4: {
                GLint val[4];
                glGetUniformiv(prog, loc, val);
                JSValueRef vals[4];
                for (int i = 0; i < 4; i++) vals[i] = JSValueMakeBoolean(ctx, val[i] != 0);
                return JSObjectMakeArray(ctx, 4, vals, nullptr);
            }
            case GL_FLOAT_MAT2: {
                GLfloat val[4];
                glGetUniformfv(prog, loc, val);
                JSValueRef vals[4];
                for (int i = 0; i < 4; i++) vals[i] = JSValueMakeNumber(ctx, val[i]);
                return JSObjectMakeArray(ctx, 4, vals, nullptr);
            }
            case GL_FLOAT_MAT3: {
                GLfloat val[9];
                glGetUniformfv(prog, loc, val);
                JSValueRef vals[9];
                for (int i = 0; i < 9; i++) vals[i] = JSValueMakeNumber(ctx, val[i]);
                return JSObjectMakeArray(ctx, 9, vals, nullptr);
            }
            case GL_FLOAT_MAT4: {
                GLfloat val[16];
                glGetUniformfv(prog, loc, val);
                JSValueRef vals[16];
                for (int i = 0; i < 16; i++) vals[i] = JSValueMakeNumber(ctx, val[i]);
                return JSObjectMakeArray(ctx, 16, vals, nullptr);
            }
            default:
                return JSValueMakeNull(ctx);
        }
    }

    // =========================================================================
    // Vertex attrib queries
    // =========================================================================

    JSValueRef GL_getVertexAttrib(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));

        switch (pname) {
            case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: {
                GLint val = 0;
                glGetVertexAttribiv(index, pname, &val);
                if (val == 0) return JSValueMakeNull(ctx);
                return MakeGLObject(ctx, "WebGLBuffer", static_cast<GLuint>(val));
            }
            case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
            case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: {
                GLint val = 0;
                glGetVertexAttribiv(index, pname, &val);
                return JSValueMakeBoolean(ctx, val != 0);
            }
            case GL_VERTEX_ATTRIB_ARRAY_SIZE:
            case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
            case GL_VERTEX_ATTRIB_ARRAY_TYPE: {
                GLint val = 0;
                glGetVertexAttribiv(index, pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
            case GL_CURRENT_VERTEX_ATTRIB: {
                GLfloat val[4] = {};
                glGetVertexAttribfv(index, pname, val);
                JSValueRef vals[4];
                for (int i = 0; i < 4; i++) vals[i] = JSValueMakeNumber(ctx, val[i]);
                return JSObjectMakeArray(ctx, 4, vals, nullptr);
            }
            default: {
                GLint val = 0;
                glGetVertexAttribiv(index, pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
        }
    }

    JSValueRef GL_getVertexAttribOffset(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNumber(ctx, 0);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        void* ptr = nullptr;
        glGetVertexAttribPointerv(index, pname, &ptr);
        return JSValueMakeNumber(ctx, static_cast<double>(reinterpret_cast<intptr_t>(ptr)));
    }

}  // namespace PrismaUI::WebGL
