#include "WebGLBridgeInternal.h"

#include <vector>
#include <string>
#include <sstream>
#include <set>

namespace PrismaUI::WebGL {

    // =========================================================================
    // GL-to-WebGL extension mapping.
    // Maps ANGLE's GL_EXTENSIONS names to their WebGL equivalents.
    // =========================================================================
    struct ExtensionMapping {
        const char* glName;       // ANGLE/GLES extension name
        const char* webglName;    // WebGL extension name
    };

    static constexpr ExtensionMapping kExtensionMap[] = {
        // WebGL 1 core extensions
        {"GL_OES_element_index_uint",            "OES_element_index_uint"},
        {"GL_OES_standard_derivatives",          "OES_standard_derivatives"},
        {"GL_OES_texture_float",                 "OES_texture_float"},
        {"GL_OES_texture_half_float",            "OES_texture_half_float"},
        {"GL_OES_texture_float_linear",          "OES_texture_float_linear"},
        {"GL_OES_texture_half_float_linear",     "OES_texture_half_float_linear"},
        {"GL_OES_vertex_array_object",           "OES_vertex_array_object"},
        {"GL_OES_fbo_render_mipmap",             "OES_fbo_render_mipmap"},
        {"GL_EXT_frag_depth",                    "EXT_frag_depth"},
        {"GL_EXT_shader_texture_lod",            "EXT_shader_texture_lod"},
        {"GL_EXT_sRGB",                          "EXT_sRGB"},
        {"GL_EXT_blend_minmax",                  "EXT_blend_minmax"},
        {"GL_EXT_color_buffer_float",            "EXT_color_buffer_float"},
        {"GL_EXT_color_buffer_half_float",       "EXT_color_buffer_half_float"},
        {"GL_EXT_float_blend",                   "EXT_float_blend"},
        {"GL_EXT_texture_filter_anisotropic",    "EXT_texture_filter_anisotropic"},
        {"GL_EXT_texture_norm16",                "EXT_texture_norm16"},
        {"GL_ANGLE_instanced_arrays",            "ANGLE_instanced_arrays"},
        {"GL_ANGLE_depth_texture",               "WEBGL_depth_texture"},
        {"GL_OES_depth_texture",                 "WEBGL_depth_texture"},
        {"GL_CHROMIUM_lose_context",             "WEBGL_lose_context"},

        // Compressed textures
        {"GL_EXT_texture_compression_s3tc",               "WEBGL_compressed_texture_s3tc"},
        {"GL_ANGLE_texture_compression_dxt3",             "WEBGL_compressed_texture_s3tc"},
        {"GL_EXT_texture_compression_s3tc_srgb",          "WEBGL_compressed_texture_s3tc_srgb"},
        {"GL_EXT_texture_compression_bptc",               "EXT_texture_compression_bptc"},
        {"GL_EXT_texture_compression_rgtc",               "EXT_texture_compression_rgtc"},
        {"GL_KHR_texture_compression_astc_ldr",           "WEBGL_compressed_texture_astc"},
        {"GL_OES_compressed_ETC2_RGBA8_texture",          "WEBGL_compressed_texture_etc"},

        // WebGL 2 / GLES 3 extensions
        {"GL_KHR_parallel_shader_compile",                "KHR_parallel_shader_compile"},
        {"GL_ANGLE_multi_draw",                           "WEBGL_multi_draw"},
        {"GL_OVR_multiview2",                             "OVR_multiview2"},
        {"GL_OES_draw_buffers_indexed",                   "OES_draw_buffers_indexed"},
        {"GL_EXT_clip_control",                           "EXT_clip_control"},
        {"GL_EXT_clip_cull_distance",                     "WEBGL_clip_cull_distance"},
        {"GL_EXT_disjoint_timer_query",                   "EXT_disjoint_timer_query_webgl2"},
        {"GL_EXT_multisampled_render_to_texture",         "WEBGL_multisampled_render_to_texture"},
        {"GL_QCOM_render_shared_exponent",                "WEBGL_render_shared_exponent"},
        {"GL_QCOM_shader_framebuffer_fetch_noncoherent",  "WEBGL_shader_pixel_local_storage"},
    };

    // Query ANGLE for actual supported extensions and return matching WebGL names.
    static std::vector<std::string> GetSupportedWebGLExtensions(ANGLEContext* c) {
        EnsureContextActive(c);
        std::set<std::string> glExts;
        const char* extStr = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        if (extStr) {
            std::istringstream iss(extStr);
            std::string ext;
            while (iss >> ext) {
                glExts.insert(ext);
            }
        }

        // Always advertise these even without explicit GL ext (GLES3 core features)
        // ANGLE with GLES 3.0 inherently supports these WebGL extensions.
        std::set<std::string> webglExts;
        webglExts.insert("OES_element_index_uint");     // GLES3 core
        webglExts.insert("WEBGL_lose_context");         // Always available (stub)
        webglExts.insert("WEBGL_depth_texture");        // GLES3 core

        for (const auto& mapping : kExtensionMap) {
            if (glExts.count(mapping.glName)) {
                webglExts.insert(mapping.webglName);
            }
        }

        return std::vector<std::string>(webglExts.begin(), webglExts.end());
    }

    static bool IsExtensionSupported(ANGLEContext* c, const std::string& webglName) {
        auto exts = GetSupportedWebGLExtensions(c);
        for (const auto& e : exts) {
            if (e == webglName) return true;
        }
        return false;
    }

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
            case GL_SAMPLES:
            case GL_MAX_SAMPLES:                              // 0x8D57 — WebGL2
            case GL_MAX_3D_TEXTURE_SIZE:                      // 0x8073 — WebGL2
            case GL_MAX_ARRAY_TEXTURE_LAYERS:                 // 0x88FF — WebGL2
            case GL_MAX_DRAW_BUFFERS:                         // 0x8824 — WebGL2
            case GL_MAX_COLOR_ATTACHMENTS:                    // 0x8CDF — WebGL2
            case GL_MAX_UNIFORM_BUFFER_BINDINGS:              // 0x8A2F — WebGL2
            case GL_MAX_VERTEX_UNIFORM_COMPONENTS:            // 0x8B4A — WebGL2
            case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:          // 0x8B49 — WebGL2
            case GL_MAX_VARYING_COMPONENTS:                   // 0x8B4B — WebGL2
            case GL_MAX_VERTEX_OUTPUT_COMPONENTS:             // 0x9122 — WebGL2
            case GL_MAX_FRAGMENT_INPUT_COMPONENTS:            // 0x9125 — WebGL2
            case GL_MAX_ELEMENTS_VERTICES:                    // 0x80E8 — WebGL2
            case GL_MAX_ELEMENTS_INDICES:                     // 0x80E9 — WebGL2
            case GL_MAX_VERTEX_UNIFORM_BLOCKS:                // 0x8A2B — WebGL2
            case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:              // 0x8A2D — WebGL2
            case GL_MAX_COMBINED_UNIFORM_BLOCKS:              // 0x8A2E — WebGL2
            case GL_MAX_UNIFORM_BLOCK_SIZE:                   // 0x8A30 — WebGL2
            case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:   // 0x8A31 — WebGL2
            case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS: // 0x8A33 — WebGL2
            case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:          // 0x8A34 — WebGL2
            case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:  // 0x8C8B — WebGL2
            case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: // 0x8C8A — WebGL2
            case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:    // 0x8C80 — WebGL2
            case GL_MAX_SERVER_WAIT_TIMEOUT:                  // 0x9111 — WebGL2
            case GL_MAX_ELEMENT_INDEX:                        // 0x8D6B — WebGL2
            case GL_MIN_PROGRAM_TEXEL_OFFSET:                 // 0x8904 — WebGL2
            case GL_MAX_PROGRAM_TEXEL_OFFSET: {               // 0x8905 — WebGL2
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
            case GL_STENCIL_TEST:
            case GL_RASTERIZER_DISCARD: {
                GLboolean val = GL_FALSE;
                glGetBooleanv(pname, &val);
                return JSValueMakeBoolean(ctx, val);
            }
            // Float parameters
            case GL_DEPTH_CLEAR_VALUE:
            case GL_LINE_WIDTH:
            case GL_POLYGON_OFFSET_FACTOR:
            case GL_POLYGON_OFFSET_UNITS:
            case GL_SAMPLE_COVERAGE_VALUE:
            case 0x84FD: // GL_MAX_TEXTURE_LOD_BIAS (WebGL2)
            case 0x84FF: { // GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
                GLfloat val = 0;
                glGetFloatv(pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
            // Float range pairs
            case GL_ALIASED_POINT_SIZE_RANGE:
            case GL_ALIASED_LINE_WIDTH_RANGE:
            case GL_DEPTH_RANGE: {
                GLfloat range[2] = {};
                glGetFloatv(pname, range);
                JSValueRef vals[2] = {
                    JSValueMakeNumber(ctx, range[0]),
                    JSValueMakeNumber(ctx, range[1])};
                return JSObjectMakeArray(ctx, 2, vals, nullptr);
            }
            // Boolean arrays
            case GL_COLOR_WRITEMASK: {
                GLboolean mask[4] = {};
                glGetBooleanv(pname, mask);
                JSValueRef vals[4];
                for (int i = 0; i < 4; i++) vals[i] = JSValueMakeBoolean(ctx, mask[i]);
                return JSObjectMakeArray(ctx, 4, vals, nullptr);
            }
            // Stencil state (integer values)
            case GL_STENCIL_FUNC:
            case GL_STENCIL_REF:
            case GL_STENCIL_VALUE_MASK:
            case GL_STENCIL_WRITEMASK:
            case GL_STENCIL_BACK_FUNC:
            case GL_STENCIL_BACK_REF:
            case GL_STENCIL_BACK_VALUE_MASK:
            case GL_STENCIL_BACK_WRITEMASK:
            case GL_STENCIL_FAIL:
            case GL_STENCIL_PASS_DEPTH_FAIL:
            case GL_STENCIL_PASS_DEPTH_PASS:
            case GL_STENCIL_BACK_FAIL:
            case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
            case GL_STENCIL_BACK_PASS_DEPTH_PASS:
            case GL_STENCIL_CLEAR_VALUE: {
                GLint val = 0;
                glGetIntegerv(pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
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

        auto exts = GetSupportedWebGLExtensions(c);
        std::vector<JSValueRef> vals;
        vals.reserve(exts.size());
        for (const auto& e : exts) {
            JSStringRef str = JSStringCreateWithUTF8CString(e.c_str());
            vals.push_back(JSValueMakeString(ctx, str));
            JSStringRelease(str);
        }
        return JSObjectMakeArray(ctx, vals.size(), vals.data(), nullptr);
    }

    // Helper to set a numeric property on a JS object
    static void SetNumericProperty(JSContextRef ctx, JSObjectRef obj, const char* key, double val) {
        JSStringRef k = JSStringCreateWithUTF8CString(key);
        JSObjectSetProperty(ctx, obj, k, JSValueMakeNumber(ctx, val), 0, nullptr);
        JSStringRelease(k);
    }

    JSValueRef GL_getExtension(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeNull(ctx);
        std::string name = GetString(ctx, argv[0]);

        // Check if this extension is actually supported by ANGLE
        if (!IsExtensionSupported(c, name)) {
            return JSValueMakeNull(ctx);
        }

        // Extensions that need specific constants on the returned object
        if (name == "EXT_texture_filter_anisotropic") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "TEXTURE_MAX_ANISOTROPY_EXT", 0x84FE);
            SetNumericProperty(ctx, ext, "MAX_TEXTURE_MAX_ANISOTROPY_EXT", 0x84FF);
            return ext;
        }

        if (name == "OES_texture_half_float") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "HALF_FLOAT_OES", 0x8D61);
            return ext;
        }

        if (name == "WEBGL_depth_texture") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "UNSIGNED_INT_24_8_WEBGL", 0x84FA);
            return ext;
        }

        if (name == "KHR_parallel_shader_compile") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "COMPLETION_STATUS_KHR", 0x91B1);
            return ext;
        }

        if (name == "WEBGL_multi_draw") {
            // TODO: needs multiDrawArrays/multiDrawElements function bindings
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            return ext;
        }

        if (name == "EXT_disjoint_timer_query_webgl2") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "QUERY_COUNTER_BITS_EXT", 0x8864);
            SetNumericProperty(ctx, ext, "TIME_ELAPSED_EXT", 0x88BF);
            SetNumericProperty(ctx, ext, "TIMESTAMP_EXT", 0x8E28);
            SetNumericProperty(ctx, ext, "GPU_DISJOINT_EXT", 0x8FBB);
            return ext;
        }

        if (name == "WEBGL_multisampled_render_to_texture") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT", 0x8D6C);
            // TODO: needs framebufferTexture2DMultisampleEXT function binding
            return ext;
        }

        if (name == "OVR_multiview2") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR", 0x9630);
            SetNumericProperty(ctx, ext, "FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR", 0x9632);
            SetNumericProperty(ctx, ext, "MAX_VIEWS_OVR", 0x9631);
            return ext;
        }

        if (name == "OES_draw_buffers_indexed") {
            // constants are already on WebGL2 context; just need truthy object
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            return ext;
        }

        if (name == "EXT_clip_control") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "LOWER_LEFT_EXT", 0x8CA1);
            SetNumericProperty(ctx, ext, "UPPER_LEFT_EXT", 0x8CA2);
            SetNumericProperty(ctx, ext, "NEGATIVE_ONE_TO_ONE_EXT", 0x935E);
            SetNumericProperty(ctx, ext, "ZERO_TO_ONE_EXT", 0x935F);
            SetNumericProperty(ctx, ext, "CLIP_ORIGIN_EXT", 0x935C);
            SetNumericProperty(ctx, ext, "CLIP_DEPTH_MODE_EXT", 0x935D);
            return ext;
        }

        if (name == "WEBGL_clip_cull_distance") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            SetNumericProperty(ctx, ext, "MAX_CLIP_DISTANCES_WEBGL", 0x0D32);
            SetNumericProperty(ctx, ext, "MAX_CULL_DISTANCES_WEBGL", 0x82F9);
            SetNumericProperty(ctx, ext, "MAX_COMBINED_CLIP_AND_CULL_DISTANCES_WEBGL", 0x82FA);
            SetNumericProperty(ctx, ext, "CLIP_DISTANCE0_WEBGL", 0x3000);
            SetNumericProperty(ctx, ext, "CLIP_DISTANCE1_WEBGL", 0x3001);
            SetNumericProperty(ctx, ext, "CLIP_DISTANCE2_WEBGL", 0x3002);
            SetNumericProperty(ctx, ext, "CLIP_DISTANCE3_WEBGL", 0x3003);
            SetNumericProperty(ctx, ext, "CLIP_DISTANCE4_WEBGL", 0x3004);
            SetNumericProperty(ctx, ext, "CLIP_DISTANCE5_WEBGL", 0x3005);
            SetNumericProperty(ctx, ext, "CLIP_DISTANCE6_WEBGL", 0x3006);
            SetNumericProperty(ctx, ext, "CLIP_DISTANCE7_WEBGL", 0x3007);
            return ext;
        }

        // WEBGL_lose_context — stub methods
        if (name == "WEBGL_lose_context") {
            JSObjectRef ext = JSObjectMake(ctx, nullptr, nullptr);
            return ext;
        }

        // Default: return a truthy empty object for any recognized extension
        return JSObjectMake(ctx, nullptr, nullptr);
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

        JSObjectRef obj = JSObjectMake(ctx, nullptr, nullptr);
        SetNumericProperty(ctx, obj, "rangeMin", range[0]);
        SetNumericProperty(ctx, obj, "rangeMax", range[1]);
        SetNumericProperty(ctx, obj, "precision", precision);
        return obj;
    }

    JSValueRef GL_getActiveAttrib(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint prog = GetGLId(ctx, argv[0]);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint maxLen = 0;
        glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLen);
        if (maxLen < 1) maxLen = 1;
        std::vector<GLchar> name(maxLen);
        GLsizei len = 0;
        GLint size = 0;
        GLenum type = 0;
        glGetActiveAttrib(prog, index, maxLen, &len, &size, &type, name.data());
        // Return WebGLActiveInfo-like object
        JSObjectRef obj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef sizeKey = JSStringCreateWithUTF8CString("size");
        JSStringRef typeKey = JSStringCreateWithUTF8CString("type");
        JSStringRef nameKey = JSStringCreateWithUTF8CString("name");
        JSObjectSetProperty(ctx, obj, sizeKey, JSValueMakeNumber(ctx, size), 0, nullptr);
        JSObjectSetProperty(ctx, obj, typeKey, JSValueMakeNumber(ctx, type), 0, nullptr);
        JSStringRef nameVal = JSStringCreateWithUTF8CString(name.data());
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
            if (uLoc != -1 && loc >= uLoc && loc < uLoc + size) {
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

    // =========================================================================
    // Uniform Buffer Object queries (WebGL2)
    // =========================================================================

    JSValueRef GL_getUniformBlockIndex(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNumber(ctx, 0xFFFFFFFF); // GL_INVALID_INDEX
        GLuint program = GetGLId(ctx, argv[0]);
        std::string name = GetString(ctx, argv[1]);
        GLuint index = glGetUniformBlockIndex(program, name.c_str());
        return JSValueMakeNumber(ctx, static_cast<double>(index));
    }

    JSValueRef GL_getActiveUniformBlockName(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint program = GetGLId(ctx, argv[0]);
        GLuint blockIndex = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));

        GLint nameLen = 0;
        glGetActiveUniformBlockiv(program, blockIndex, GL_UNIFORM_BLOCK_NAME_LENGTH, &nameLen);
        if (nameLen <= 0) return JSValueMakeNull(ctx);

        std::vector<char> buf(nameLen);
        glGetActiveUniformBlockName(program, blockIndex, nameLen, nullptr, buf.data());
        JSStringRef str = JSStringCreateWithUTF8CString(buf.data());
        JSValueRef result = JSValueMakeString(ctx, str);
        JSStringRelease(str);
        return result;
    }

    JSValueRef GL_getActiveUniformBlockParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeNull(ctx);
        GLuint program = GetGLId(ctx, argv[0]);
        GLuint blockIndex = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr));

        switch (pname) {
            case GL_UNIFORM_BLOCK_BINDING:
            case GL_UNIFORM_BLOCK_DATA_SIZE:
            case GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS: {
                GLint val = 0;
                glGetActiveUniformBlockiv(program, blockIndex, pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
            case GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
            case GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER: {
                GLint val = 0;
                glGetActiveUniformBlockiv(program, blockIndex, pname, &val);
                return JSValueMakeBoolean(ctx, val != 0);
            }
            case GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: {
                GLint count = 0;
                glGetActiveUniformBlockiv(program, blockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &count);
                if (count <= 0) return JSObjectMakeArray(ctx, 0, nullptr, nullptr);
                std::vector<GLint> indices(count);
                glGetActiveUniformBlockiv(program, blockIndex, pname, indices.data());
                std::vector<JSValueRef> vals(count);
                for (GLint i = 0; i < count; i++)
                    vals[i] = JSValueMakeNumber(ctx, static_cast<double>(indices[i]));
                return JSObjectMakeArray(ctx, static_cast<size_t>(count), vals.data(), nullptr);
            }
            default: {
                GLint val = 0;
                glGetActiveUniformBlockiv(program, blockIndex, pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
        }
    }

    JSValueRef GL_getUniformIndices(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint program = GetGLId(ctx, argv[0]);

        // argv[1] is a JS array of strings
        JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
        if (!arr) return JSValueMakeNull(ctx);

        JSStringRef lengthKey = JSStringCreateWithUTF8CString("length");
        GLsizei count = static_cast<GLsizei>(
            JSValueToNumber(ctx, JSObjectGetProperty(ctx, arr, lengthKey, nullptr), nullptr));
        JSStringRelease(lengthKey);

        std::vector<std::string> names(count);
        std::vector<const char*> namesPtrs(count);
        for (GLsizei i = 0; i < count; i++) {
            JSValueRef elem = JSObjectGetPropertyAtIndex(ctx, arr, i, nullptr);
            names[i] = GetString(ctx, elem);
            namesPtrs[i] = names[i].c_str();
        }

        std::vector<GLuint> indices(count);
        glGetUniformIndices(program, count, namesPtrs.data(), indices.data());

        std::vector<JSValueRef> vals(count);
        for (GLsizei i = 0; i < count; i++)
            vals[i] = JSValueMakeNumber(ctx, static_cast<double>(indices[i]));
        return JSObjectMakeArray(ctx, static_cast<size_t>(count), vals.data(), nullptr);
    }

    JSValueRef GL_getActiveUniforms(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeNull(ctx);
        GLuint program = GetGLId(ctx, argv[0]);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr));

        // argv[1] is a JS array of uniform indices
        JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
        if (!arr) return JSValueMakeNull(ctx);

        JSStringRef lengthKey = JSStringCreateWithUTF8CString("length");
        GLsizei count = static_cast<GLsizei>(
            JSValueToNumber(ctx, JSObjectGetProperty(ctx, arr, lengthKey, nullptr), nullptr));
        JSStringRelease(lengthKey);

        std::vector<GLuint> uniformIndices(count);
        for (GLsizei i = 0; i < count; i++) {
            JSValueRef elem = JSObjectGetPropertyAtIndex(ctx, arr, i, nullptr);
            uniformIndices[i] = static_cast<GLuint>(JSValueToNumber(ctx, elem, nullptr));
        }

        std::vector<GLint> params(count);
        glGetActiveUniformsiv(program, count, uniformIndices.data(), pname, params.data());

        std::vector<JSValueRef> vals(count);
        for (GLsizei i = 0; i < count; i++)
            vals[i] = JSValueMakeNumber(ctx, static_cast<double>(params[i]));
        return JSObjectMakeArray(ctx, static_cast<size_t>(count), vals.data(), nullptr);
    }

    // =========================================================================
    // Misc Queries (WebGL2)
    // =========================================================================

    JSValueRef GL_getFragDataLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNumber(ctx, -1);
        GLuint program = GetGLId(ctx, argv[0]);
        std::string name = GetString(ctx, argv[1]);
        GLint loc = glGetFragDataLocation(program, name.c_str());
        return JSValueMakeNumber(ctx, static_cast<double>(loc));
    }

    JSValueRef GL_getIndexedParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));

        switch (target) {
            case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
            case GL_UNIFORM_BUFFER_BINDING: {
                GLint val = 0;
                glGetIntegeri_v(target, index, &val);
                if (val == 0) return JSValueMakeNull(ctx);
                return MakeGLObject(ctx, "WebGLBuffer", static_cast<GLuint>(val));
            }
            case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
            case GL_TRANSFORM_FEEDBACK_BUFFER_START:
            case GL_UNIFORM_BUFFER_SIZE:
            case GL_UNIFORM_BUFFER_START: {
                GLint64 val64 = 0;
                glGetInteger64i_v(target, index, &val64);
                return JSValueMakeNumber(ctx, static_cast<double>(val64));
            }
            default: {
                GLint val = 0;
                glGetIntegeri_v(target, index, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
        }
    }

}  // namespace PrismaUI::WebGL
