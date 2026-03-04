#include "WebGLBridge.h"
#include "WebGLBridgeInternal.h"

#include "ANGLEContext.h"
#include "PrismaUI/Core.h"

#include <JavaScriptCore/JavaScript.h>
#include <spdlog/spdlog.h>

#include <d3d11.h>

#include <chrono>
#include <shared_mutex>
#include <string>

namespace PrismaUI::WebGL {

    // =========================================================================
    // JSClass definition for WebGLRenderingContext
    // =========================================================================

    static JSStaticFunction kWebGLFunctions[] = {
        // Context/State
        {"getError",                    GL_getError,                    kJSPropertyAttributeDontDelete},
        {"enable",                      GL_enable,                      kJSPropertyAttributeDontDelete},
        {"disable",                     GL_disable,                     kJSPropertyAttributeDontDelete},
        {"viewport",                    GL_viewport,                    kJSPropertyAttributeDontDelete},
        {"scissor",                     GL_scissor,                     kJSPropertyAttributeDontDelete},
        {"clearColor",                  GL_clearColor,                  kJSPropertyAttributeDontDelete},
        {"clearDepth",                  GL_clearDepth,                  kJSPropertyAttributeDontDelete},
        {"clearStencil",                GL_clearStencil,                kJSPropertyAttributeDontDelete},
        {"clear",                       GL_clear,                       kJSPropertyAttributeDontDelete},
        {"colorMask",                   GL_colorMask,                   kJSPropertyAttributeDontDelete},
        {"depthFunc",                   GL_depthFunc,                   kJSPropertyAttributeDontDelete},
        {"depthMask",                   GL_depthMask,                   kJSPropertyAttributeDontDelete},
        {"depthRange",                  GL_depthRange,                  kJSPropertyAttributeDontDelete},
        {"pixelStorei",                 GL_pixelStorei,                 kJSPropertyAttributeDontDelete},
        {"flush",                       GL_flush,                       kJSPropertyAttributeDontDelete},
        {"finish",                      GL_finish,                      kJSPropertyAttributeDontDelete},
        {"getParameter",                GL_getParameter,                kJSPropertyAttributeDontDelete},
        {"getSupportedExtensions",      GL_getSupportedExtensions,      kJSPropertyAttributeDontDelete},
        {"getExtension",                GL_getExtension,                kJSPropertyAttributeDontDelete},
        {"isContextLost",               GL_isContextLost,               kJSPropertyAttributeDontDelete},
        {"getShaderPrecisionFormat",    GL_getShaderPrecisionFormat,    kJSPropertyAttributeDontDelete},
        // Blend
        {"blendFunc",                   GL_blendFunc,                   kJSPropertyAttributeDontDelete},
        {"blendFuncSeparate",           GL_blendFuncSeparate,           kJSPropertyAttributeDontDelete},
        {"blendEquation",               GL_blendEquation,               kJSPropertyAttributeDontDelete},
        {"blendEquationSeparate",       GL_blendEquationSeparate,       kJSPropertyAttributeDontDelete},
        {"blendColor",                  GL_blendColor,                  kJSPropertyAttributeDontDelete},
        // Culling
        {"cullFace",                    GL_cullFace,                    kJSPropertyAttributeDontDelete},
        {"frontFace",                   GL_frontFace,                   kJSPropertyAttributeDontDelete},
        {"lineWidth",                   GL_lineWidth,                   kJSPropertyAttributeDontDelete},
        {"polygonOffset",               GL_polygonOffset,               kJSPropertyAttributeDontDelete},
        // Buffers
        {"createBuffer",                GL_createBuffer,                kJSPropertyAttributeDontDelete},
        {"deleteBuffer",                GL_deleteBuffer,                kJSPropertyAttributeDontDelete},
        {"bindBuffer",                  GL_bindBuffer,                  kJSPropertyAttributeDontDelete},
        {"bufferData",                  GL_bufferData,                  kJSPropertyAttributeDontDelete},
        {"bufferSubData",               GL_bufferSubData,               kJSPropertyAttributeDontDelete},
        // Shaders
        {"createShader",                GL_createShader,                kJSPropertyAttributeDontDelete},
        {"deleteShader",                GL_deleteShader,                kJSPropertyAttributeDontDelete},
        {"shaderSource",                GL_shaderSource,                kJSPropertyAttributeDontDelete},
        {"compileShader",               GL_compileShader,               kJSPropertyAttributeDontDelete},
        {"getShaderParameter",          GL_getShaderParameter,          kJSPropertyAttributeDontDelete},
        {"getShaderInfoLog",            GL_getShaderInfoLog,            kJSPropertyAttributeDontDelete},
        {"createProgram",               GL_createProgram,               kJSPropertyAttributeDontDelete},
        {"deleteProgram",               GL_deleteProgram,               kJSPropertyAttributeDontDelete},
        {"attachShader",                GL_attachShader,                kJSPropertyAttributeDontDelete},
        {"detachShader",                GL_detachShader,                kJSPropertyAttributeDontDelete},
        {"linkProgram",                 GL_linkProgram,                 kJSPropertyAttributeDontDelete},
        {"useProgram",                  GL_useProgram,                  kJSPropertyAttributeDontDelete},
        {"getProgramParameter",         GL_getProgramParameter,         kJSPropertyAttributeDontDelete},
        {"getProgramInfoLog",           GL_getProgramInfoLog,           kJSPropertyAttributeDontDelete},
        {"validateProgram",             GL_validateProgram,             kJSPropertyAttributeDontDelete},
        {"bindAttribLocation",          GL_bindAttribLocation,          kJSPropertyAttributeDontDelete},
        {"getActiveAttrib",             GL_getActiveAttrib,             kJSPropertyAttributeDontDelete},
        {"getActiveUniform",            GL_getActiveUniform,            kJSPropertyAttributeDontDelete},
        // Attributes
        {"getAttribLocation",           GL_getAttribLocation,           kJSPropertyAttributeDontDelete},
        {"enableVertexAttribArray",     GL_enableVertexAttribArray,     kJSPropertyAttributeDontDelete},
        {"disableVertexAttribArray",    GL_disableVertexAttribArray,    kJSPropertyAttributeDontDelete},
        {"vertexAttribPointer",         GL_vertexAttribPointer,         kJSPropertyAttributeDontDelete},
        // Uniforms
        {"getUniformLocation",          GL_getUniformLocation,          kJSPropertyAttributeDontDelete},
        {"uniform1f",                   GL_uniform1f,                   kJSPropertyAttributeDontDelete},
        {"uniform1i",                   GL_uniform1i,                   kJSPropertyAttributeDontDelete},
        {"uniform2f",                   GL_uniform2f,                   kJSPropertyAttributeDontDelete},
        {"uniform3f",                   GL_uniform3f,                   kJSPropertyAttributeDontDelete},
        {"uniform4f",                   GL_uniform4f,                   kJSPropertyAttributeDontDelete},
        {"uniform1fv",                  GL_uniform1fv,                  kJSPropertyAttributeDontDelete},
        {"uniform2fv",                  GL_uniform2fv,                  kJSPropertyAttributeDontDelete},
        {"uniform3fv",                  GL_uniform3fv,                  kJSPropertyAttributeDontDelete},
        {"uniform4fv",                  GL_uniform4fv,                  kJSPropertyAttributeDontDelete},
        {"uniform1iv",                  GL_uniform1iv,                  kJSPropertyAttributeDontDelete},
        {"uniform2i",                   GL_uniform2i,                   kJSPropertyAttributeDontDelete},
        {"uniform2iv",                  GL_uniform2iv,                  kJSPropertyAttributeDontDelete},
        {"uniform3i",                   GL_uniform3i,                   kJSPropertyAttributeDontDelete},
        {"uniform3iv",                  GL_uniform3iv,                  kJSPropertyAttributeDontDelete},
        {"uniform4i",                   GL_uniform4i,                   kJSPropertyAttributeDontDelete},
        {"uniform4iv",                  GL_uniform4iv,                  kJSPropertyAttributeDontDelete},
        {"uniformMatrix2fv",            GL_uniformMatrix2fv,            kJSPropertyAttributeDontDelete},
        {"uniformMatrix3fv",            GL_uniformMatrix3fv,            kJSPropertyAttributeDontDelete},
        {"uniformMatrix4fv",            GL_uniformMatrix4fv,            kJSPropertyAttributeDontDelete},
        // Textures
        {"createTexture",               GL_createTexture,               kJSPropertyAttributeDontDelete},
        {"deleteTexture",               GL_deleteTexture,               kJSPropertyAttributeDontDelete},
        {"bindTexture",                 GL_bindTexture,                 kJSPropertyAttributeDontDelete},
        {"activeTexture",               GL_activeTexture,               kJSPropertyAttributeDontDelete},
        {"texParameteri",               GL_texParameteri,               kJSPropertyAttributeDontDelete},
        {"texParameterf",               GL_texParameterf,               kJSPropertyAttributeDontDelete},
        {"generateMipmap",              GL_generateMipmap,              kJSPropertyAttributeDontDelete},
        {"texImage2D",                  GL_texImage2D,                  kJSPropertyAttributeDontDelete},
        {"texSubImage2D",               GL_texSubImage2D,               kJSPropertyAttributeDontDelete},
        {"copyTexImage2D",              GL_copyTexImage2D,              kJSPropertyAttributeDontDelete},
        {"copyTexSubImage2D",           GL_copyTexSubImage2D,           kJSPropertyAttributeDontDelete},
        // Framebuffers
        {"createFramebuffer",           GL_createFramebuffer,           kJSPropertyAttributeDontDelete},
        {"deleteFramebuffer",           GL_deleteFramebuffer,           kJSPropertyAttributeDontDelete},
        {"bindFramebuffer",             GL_bindFramebuffer,             kJSPropertyAttributeDontDelete},
        {"framebufferTexture2D",        GL_framebufferTexture2D,        kJSPropertyAttributeDontDelete},
        {"checkFramebufferStatus",      GL_checkFramebufferStatus,      kJSPropertyAttributeDontDelete},
        {"framebufferRenderbuffer",     GL_framebufferRenderbuffer,     kJSPropertyAttributeDontDelete},
        // Renderbuffers
        {"createRenderbuffer",          GL_createRenderbuffer,          kJSPropertyAttributeDontDelete},
        {"deleteRenderbuffer",          GL_deleteRenderbuffer,          kJSPropertyAttributeDontDelete},
        {"bindRenderbuffer",            GL_bindRenderbuffer,            kJSPropertyAttributeDontDelete},
        {"renderbufferStorage",         GL_renderbufferStorage,         kJSPropertyAttributeDontDelete},
        // Drawing
        {"drawArrays",                  GL_drawArrays,                  kJSPropertyAttributeDontDelete},
        {"drawElements",                GL_drawElements,                kJSPropertyAttributeDontDelete},
        // Stencil
        {"stencilFunc",                 GL_stencilFunc,                 kJSPropertyAttributeDontDelete},
        {"stencilFuncSeparate",         GL_stencilFuncSeparate,         kJSPropertyAttributeDontDelete},
        {"stencilOp",                   GL_stencilOp,                   kJSPropertyAttributeDontDelete},
        {"stencilOpSeparate",           GL_stencilOpSeparate,           kJSPropertyAttributeDontDelete},
        {"stencilMask",                 GL_stencilMask,                 kJSPropertyAttributeDontDelete},
        {"stencilMaskSeparate",         GL_stencilMaskSeparate,         kJSPropertyAttributeDontDelete},
        // Misc State
        {"hint",                        GL_hint,                        kJSPropertyAttributeDontDelete},
        {"sampleCoverage",              GL_sampleCoverage,              kJSPropertyAttributeDontDelete},
        // Reading
        {"readPixels",                  GL_readPixels,                  kJSPropertyAttributeDontDelete},
        // Vertex Attrib Constants
        {"vertexAttrib1f",              GL_vertexAttrib1f,              kJSPropertyAttributeDontDelete},
        {"vertexAttrib2f",              GL_vertexAttrib2f,              kJSPropertyAttributeDontDelete},
        {"vertexAttrib3f",              GL_vertexAttrib3f,              kJSPropertyAttributeDontDelete},
        {"vertexAttrib4f",              GL_vertexAttrib4f,              kJSPropertyAttributeDontDelete},
        {"vertexAttrib1fv",             GL_vertexAttrib1fv,             kJSPropertyAttributeDontDelete},
        {"vertexAttrib2fv",             GL_vertexAttrib2fv,             kJSPropertyAttributeDontDelete},
        {"vertexAttrib3fv",             GL_vertexAttrib3fv,             kJSPropertyAttributeDontDelete},
        {"vertexAttrib4fv",             GL_vertexAttrib4fv,             kJSPropertyAttributeDontDelete},
        // Boolean queries
        {"isEnabled",                   GL_isEnabled,                   kJSPropertyAttributeDontDelete},
        {"isBuffer",                    GL_isBuffer,                    kJSPropertyAttributeDontDelete},
        {"isFramebuffer",               GL_isFramebuffer,               kJSPropertyAttributeDontDelete},
        {"isRenderbuffer",              GL_isRenderbuffer,              kJSPropertyAttributeDontDelete},
        {"isTexture",                   GL_isTexture,                   kJSPropertyAttributeDontDelete},
        {"isProgram",                   GL_isProgram,                   kJSPropertyAttributeDontDelete},
        {"isShader",                    GL_isShader,                    kJSPropertyAttributeDontDelete},
        // State queries
        {"getBufferParameter",          GL_getBufferParameter,          kJSPropertyAttributeDontDelete},
        {"getRenderbufferParameter",    GL_getRenderbufferParameter,    kJSPropertyAttributeDontDelete},
        {"getTexParameter",             GL_getTexParameter,             kJSPropertyAttributeDontDelete},
        {"getFramebufferAttachmentParameter", GL_getFramebufferAttachmentParameter, kJSPropertyAttributeDontDelete},
        {"getContextAttributes",        GL_getContextAttributes,        kJSPropertyAttributeDontDelete},
        {"getShaderSource",             GL_getShaderSource,             kJSPropertyAttributeDontDelete},
        {"getAttachedShaders",          GL_getAttachedShaders,          kJSPropertyAttributeDontDelete},
        {"getUniform",                  GL_getUniform,                  kJSPropertyAttributeDontDelete},
        {"getVertexAttrib",             GL_getVertexAttrib,             kJSPropertyAttributeDontDelete},
        {"getVertexAttribOffset",       GL_getVertexAttribOffset,       kJSPropertyAttributeDontDelete},
        // WebGL2: Vertex Array Objects
        {"createVertexArray",           GL_createVertexArray,           kJSPropertyAttributeDontDelete},
        {"deleteVertexArray",           GL_deleteVertexArray,           kJSPropertyAttributeDontDelete},
        {"bindVertexArray",             GL_bindVertexArray,             kJSPropertyAttributeDontDelete},
        {"isVertexArray",               GL_isVertexArray,               kJSPropertyAttributeDontDelete},
        // WebGL2: Instanced drawing
        {"drawArraysInstanced",         GL_drawArraysInstanced,         kJSPropertyAttributeDontDelete},
        {"drawElementsInstanced",       GL_drawElementsInstanced,       kJSPropertyAttributeDontDelete},
        {"drawRangeElements",           GL_drawRangeElements,           kJSPropertyAttributeDontDelete},
        {"vertexAttribDivisor",         GL_vertexAttribDivisor,         kJSPropertyAttributeDontDelete},
        // WebGL2: Uniform Buffer Objects
        {"bindBufferBase",              GL_bindBufferBase,              kJSPropertyAttributeDontDelete},
        {"bindBufferRange",             GL_bindBufferRange,             kJSPropertyAttributeDontDelete},
        {"uniformBlockBinding",         GL_uniformBlockBinding,         kJSPropertyAttributeDontDelete},
        {"getUniformBlockIndex",        GL_getUniformBlockIndex,        kJSPropertyAttributeDontDelete},
        {"getActiveUniformBlockName",   GL_getActiveUniformBlockName,   kJSPropertyAttributeDontDelete},
        {"getActiveUniformBlockParameter", GL_getActiveUniformBlockParameter, kJSPropertyAttributeDontDelete},
        {"getUniformIndices",           GL_getUniformIndices,           kJSPropertyAttributeDontDelete},
        {"getActiveUniforms",           GL_getActiveUniforms,           kJSPropertyAttributeDontDelete},
        // WebGL2: Framebuffer enhancements
        {"drawBuffers",                 GL_drawBuffers,                 kJSPropertyAttributeDontDelete},
        {"readBuffer",                  GL_readBuffer,                  kJSPropertyAttributeDontDelete},
        {"blitFramebuffer",             GL_blitFramebuffer,             kJSPropertyAttributeDontDelete},
        {"framebufferTextureLayer",     GL_framebufferTextureLayer,     kJSPropertyAttributeDontDelete},
        {"renderbufferStorageMultisample", GL_renderbufferStorageMultisample, kJSPropertyAttributeDontDelete},
        {"invalidateFramebuffer",       GL_invalidateFramebuffer,       kJSPropertyAttributeDontDelete},
        {"invalidateSubFramebuffer",    GL_invalidateSubFramebuffer,    kJSPropertyAttributeDontDelete},
        // WebGL2: Transform Feedback
        {"createTransformFeedback",     GL_createTransformFeedback,     kJSPropertyAttributeDontDelete},
        {"deleteTransformFeedback",     GL_deleteTransformFeedback,     kJSPropertyAttributeDontDelete},
        {"bindTransformFeedback",       GL_bindTransformFeedback,       kJSPropertyAttributeDontDelete},
        {"isTransformFeedback",         GL_isTransformFeedback,         kJSPropertyAttributeDontDelete},
        {"beginTransformFeedback",      GL_beginTransformFeedback,      kJSPropertyAttributeDontDelete},
        {"endTransformFeedback",        GL_endTransformFeedback,        kJSPropertyAttributeDontDelete},
        {"pauseTransformFeedback",      GL_pauseTransformFeedback,      kJSPropertyAttributeDontDelete},
        {"resumeTransformFeedback",     GL_resumeTransformFeedback,     kJSPropertyAttributeDontDelete},
        {"transformFeedbackVaryings",   GL_transformFeedbackVaryings,   kJSPropertyAttributeDontDelete},
        {"getTransformFeedbackVarying", GL_getTransformFeedbackVarying, kJSPropertyAttributeDontDelete},
        // WebGL2: Uint uniforms
        {"uniform1ui",                  GL_uniform1ui,                  kJSPropertyAttributeDontDelete},
        {"uniform2ui",                  GL_uniform2ui,                  kJSPropertyAttributeDontDelete},
        {"uniform3ui",                  GL_uniform3ui,                  kJSPropertyAttributeDontDelete},
        {"uniform4ui",                  GL_uniform4ui,                  kJSPropertyAttributeDontDelete},
        {"uniform1uiv",                 GL_uniform1uiv,                 kJSPropertyAttributeDontDelete},
        {"uniform2uiv",                 GL_uniform2uiv,                 kJSPropertyAttributeDontDelete},
        {"uniform3uiv",                 GL_uniform3uiv,                 kJSPropertyAttributeDontDelete},
        {"uniform4uiv",                 GL_uniform4uiv,                 kJSPropertyAttributeDontDelete},
        // WebGL2: Non-square matrix uniforms
        {"uniformMatrix2x3fv",          GL_uniformMatrix2x3fv,          kJSPropertyAttributeDontDelete},
        {"uniformMatrix3x2fv",          GL_uniformMatrix3x2fv,          kJSPropertyAttributeDontDelete},
        {"uniformMatrix2x4fv",          GL_uniformMatrix2x4fv,          kJSPropertyAttributeDontDelete},
        {"uniformMatrix4x2fv",          GL_uniformMatrix4x2fv,          kJSPropertyAttributeDontDelete},
        {"uniformMatrix3x4fv",          GL_uniformMatrix3x4fv,          kJSPropertyAttributeDontDelete},
        {"uniformMatrix4x3fv",          GL_uniformMatrix4x3fv,          kJSPropertyAttributeDontDelete},
        // WebGL2: Integer vertex attribs
        {"vertexAttribIPointer",        GL_vertexAttribIPointer,        kJSPropertyAttributeDontDelete},
        {"vertexAttribI4i",             GL_vertexAttribI4i,             kJSPropertyAttributeDontDelete},
        {"vertexAttribI4ui",            GL_vertexAttribI4ui,            kJSPropertyAttributeDontDelete},
        {"vertexAttribI4iv",            GL_vertexAttribI4iv,            kJSPropertyAttributeDontDelete},
        {"vertexAttribI4uiv",           GL_vertexAttribI4uiv,           kJSPropertyAttributeDontDelete},
        // WebGL2: Clear buffer
        {"clearBufferiv",               GL_clearBufferiv,               kJSPropertyAttributeDontDelete},
        {"clearBufferuiv",              GL_clearBufferuiv,              kJSPropertyAttributeDontDelete},
        {"clearBufferfv",               GL_clearBufferfv,               kJSPropertyAttributeDontDelete},
        {"clearBufferfi",               GL_clearBufferfi,               kJSPropertyAttributeDontDelete},
        // WebGL2: 3D Textures & Storage
        {"texStorage2D",                GL_texStorage2D,                kJSPropertyAttributeDontDelete},
        {"texStorage3D",                GL_texStorage3D,                kJSPropertyAttributeDontDelete},
        {"texImage3D",                  GL_texImage3D,                  kJSPropertyAttributeDontDelete},
        {"texSubImage3D",               GL_texSubImage3D,               kJSPropertyAttributeDontDelete},
        {"copyTexSubImage3D",           GL_copyTexSubImage3D,           kJSPropertyAttributeDontDelete},
        // WebGL2: Samplers
        {"createSampler",               GL_createSampler,               kJSPropertyAttributeDontDelete},
        {"deleteSampler",               GL_deleteSampler,               kJSPropertyAttributeDontDelete},
        {"bindSampler",                 GL_bindSampler,                 kJSPropertyAttributeDontDelete},
        {"isSampler",                   GL_isSampler,                   kJSPropertyAttributeDontDelete},
        {"samplerParameteri",           GL_samplerParameteri,           kJSPropertyAttributeDontDelete},
        {"samplerParameterf",           GL_samplerParameterf,           kJSPropertyAttributeDontDelete},
        {"getSamplerParameter",         GL_getSamplerParameter,         kJSPropertyAttributeDontDelete},
        // WebGL2: Queries
        {"createQuery",                 GL_createQuery,                 kJSPropertyAttributeDontDelete},
        {"deleteQuery",                 GL_deleteQuery,                 kJSPropertyAttributeDontDelete},
        {"isQuery",                     GL_isQuery,                     kJSPropertyAttributeDontDelete},
        {"beginQuery",                  GL_beginQuery,                  kJSPropertyAttributeDontDelete},
        {"endQuery",                    GL_endQuery,                    kJSPropertyAttributeDontDelete},
        {"getQuery",                    GL_getQuery,                    kJSPropertyAttributeDontDelete},
        {"getQueryParameter",           GL_getQueryParameter,           kJSPropertyAttributeDontDelete},
        // WebGL2: Sync
        {"fenceSync",                   GL_fenceSync,                   kJSPropertyAttributeDontDelete},
        {"isSync",                      GL_isSync,                      kJSPropertyAttributeDontDelete},
        {"deleteSync",                  GL_deleteSync,                  kJSPropertyAttributeDontDelete},
        {"clientWaitSync",              GL_clientWaitSync,              kJSPropertyAttributeDontDelete},
        {"waitSync",                    GL_waitSync,                    kJSPropertyAttributeDontDelete},
        {"getSyncParameter",            GL_getSyncParameter,            kJSPropertyAttributeDontDelete},
        // WebGL2: Buffer operations
        {"copyBufferSubData",           GL_copyBufferSubData,           kJSPropertyAttributeDontDelete},
        {"getBufferSubData",            GL_getBufferSubData,            kJSPropertyAttributeDontDelete},
        // WebGL2: Misc queries
        {"getFragDataLocation",         GL_getFragDataLocation,         kJSPropertyAttributeDontDelete},
        {"getIndexedParameter",         GL_getIndexedParameter,         kJSPropertyAttributeDontDelete},
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
    // Native function: __prismaCreateWebGLContext(width, height, x, y)
    // Called from the JS shim when canvas.getContext('webgl') is invoked.
    // =========================================================================
    static JSValueRef JS_CreateWebGLContext(JSContextRef ctx, JSObjectRef function,
                                            JSObjectRef /*thisObject*/, size_t argc,
                                            const JSValueRef argv[], JSValueRef* exc) {
        if (argc < 2) {
            logger::error("[WebGL] __prismaCreateWebGLContext called with insufficient args");
            return JSValueMakeNull(ctx);
        }

        uint32_t width = static_cast<uint32_t>(JSValueToNumber(ctx, argv[0], exc));
        uint32_t height = static_cast<uint32_t>(JSValueToNumber(ctx, argv[1], exc));

        if (width == 0 || height == 0) {
            width = width ? width : 300;
            height = height ? height : 150;
        }

        // Optional canvas position args (set by the JS shim from getBoundingClientRect)
        float canvasX = 0.0f, canvasY = 0.0f;
        if (argc >= 4) {
            canvasX = static_cast<float>(JSValueToNumber(ctx, argv[2], nullptr));
            canvasY = static_cast<float>(JSValueToNumber(ctx, argv[3], nullptr));
        }

        // Get the D3D device from Core
        ID3D11Device* device = PrismaUI::Core::d3dDevice;

        if (!device) {
            logger::error("[WebGL] D3D11 device not available");
            return JSValueMakeNull(ctx);
        }

        ANGLEContext* angleCtx = CreateWebGLContext(width, height, device);
        if (!angleCtx) {
            logger::error("[WebGL] Failed to create ANGLE context");
            return JSValueMakeNull(ctx);
        }

        angleCtx->canvasX = canvasX;
        angleCtx->canvasY = canvasY;
        angleCtx->visible = true;
        angleCtx->lastUpdateMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

        // Link the ANGLE context to the owning PrismaView so the render thread
        // can composite the shared D3D11 texture onto the Ultralight view.
        // viewId is stored as a string (not a number) to preserve full uint64_t precision.
        JSStringRef viewIdProp = JSStringCreateWithUTF8CString("__viewId");
        JSValueRef viewIdVal = JSObjectGetProperty(ctx, function, viewIdProp, nullptr);
        JSStringRelease(viewIdProp);

        if (JSValueIsString(ctx, viewIdVal)) {
            JSStringRef viewIdJSStr = JSValueToStringCopy(ctx, viewIdVal, nullptr);
            size_t bufLen = JSStringGetMaximumUTF8CStringSize(viewIdJSStr);
            std::string viewIdBuf(bufLen, '\0');
            JSStringGetUTF8CString(viewIdJSStr, viewIdBuf.data(), bufLen);
            JSStringRelease(viewIdJSStr);

            auto viewId = static_cast<uint64_t>(std::strtoull(viewIdBuf.c_str(), nullptr, 10));
            std::shared_lock lock(Core::viewsMutex);
            auto it = Core::views.find(viewId);
            if (it != Core::views.end()) {
                it->second->webglContext = angleCtx;
                logger::info("[WebGL] ANGLE context linked to PrismaView {}", viewId);
            }
        }

        // Create a JSC object of the WebGLRenderingContext class with angleCtx as private data
        JSClassRef webglClass = GetWebGLContextClass();
        JSObjectRef contextObj = JSObjectMake(ctx, webglClass, angleCtx);

        logger::info("[WebGL] WebGL context created and bound to JS: {}x{}", width, height);
        return contextObj;
    }

    // =========================================================================
    // Native function: __prismaUpdateWebGLContext(ctx, x, y, w, h, visible)
    // Called by the JS shim to keep position/visibility in sync.
    // =========================================================================
    static JSValueRef JS_UpdateWebGLContext(JSContextRef ctx, JSObjectRef /*function*/,
                                            JSObjectRef /*thisObject*/, size_t argc,
                                            const JSValueRef argv[], JSValueRef* /*exc*/) {
        if (argc < 6) {
            return JSValueMakeUndefined(ctx);
        }

        JSObjectRef ctxObj = JSValueToObject(ctx, argv[0], nullptr);
        if (!ctxObj) {
            return JSValueMakeUndefined(ctx);
        }

        auto* angleCtx = static_cast<ANGLEContext*>(JSObjectGetPrivate(ctxObj));
        if (!angleCtx || !angleCtx->initialized) {
            return JSValueMakeUndefined(ctx);
        }

        float canvasX = static_cast<float>(JSValueToNumber(ctx, argv[1], nullptr));
        float canvasY = static_cast<float>(JSValueToNumber(ctx, argv[2], nullptr));
        uint32_t width = static_cast<uint32_t>(JSValueToNumber(ctx, argv[3], nullptr));
        uint32_t height = static_cast<uint32_t>(JSValueToNumber(ctx, argv[4], nullptr));
        bool visible = JSValueToBoolean(ctx, argv[5]);

        // Optional display size args (argv[6], argv[7]) — CSS display dimensions.
        // When the canvas CSS size differs from its buffer size (e.g. object-fit),
        // the display size determines how large the overlay is drawn on screen.
        float displayW = static_cast<float>(width);   // default: same as buffer
        float displayH = static_cast<float>(height);
        if (argc >= 8) {
            float dw = static_cast<float>(JSValueToNumber(ctx, argv[6], nullptr));
            float dh = static_cast<float>(JSValueToNumber(ctx, argv[7], nullptr));
            if (dw > 0 && dh > 0) {
                displayW = dw;
                displayH = dh;
            }
        }

        angleCtx->canvasX = canvasX;
        angleCtx->canvasY = canvasY;
        angleCtx->displayWidth = displayW;
        angleCtx->displayHeight = displayH;
        angleCtx->visible = visible;
        angleCtx->lastUpdateMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

        // One-time log to confirm the JS update loop is running
        static bool loggedFirstUpdate = false;
        if (!loggedFirstUpdate) {
            logger::info("[WebGL-DBG] JS_UpdateWebGLContext: first update call — pos=({},{}) bufSize={}x{} displaySize={}x{} visible={}",
                canvasX, canvasY, width, height, displayW, displayH, visible);
            loggedFirstUpdate = true;
        }

        if (width > 0 && height > 0 &&
            (width != angleCtx->canvasWidth || height != angleCtx->canvasHeight)) {
            ID3D11Device* device = PrismaUI::Core::d3dDevice;
            if (device) {
                ResizeWebGLContext(angleCtx, width, height, device);
            }
        }

        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // InjectWebGLBindings: called from OnWindowObjectReady
    // =========================================================================
    void InjectWebGLBindings(JSContextRef jsCtx, uint64_t viewId) {
        JSObjectRef globalObj = JSContextGetGlobalObject(jsCtx);

        // Bind __prismaCreateWebGLContext as a global function
        JSStringRef funcName = JSStringCreateWithUTF8CString("__prismaCreateWebGLContext");
        JSObjectRef funcObj = JSObjectMakeFunctionWithCallback(jsCtx, funcName, JS_CreateWebGLContext);

        // Store viewId as a STRING on the function object (not a number — viewIds
        // exceed JavaScript's 2^53 safe integer limit and lose precision as doubles).
        std::string viewIdStr = std::to_string(viewId);
        JSStringRef viewIdPropStr = JSStringCreateWithUTF8CString("__viewId");
        JSStringRef viewIdJSStr = JSStringCreateWithUTF8CString(viewIdStr.c_str());
        JSObjectSetProperty(jsCtx, funcObj, viewIdPropStr,
                            JSValueMakeString(jsCtx, viewIdJSStr),
                            kJSPropertyAttributeDontEnum | kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(viewIdJSStr);
        JSStringRelease(viewIdPropStr);

        JSObjectSetProperty(jsCtx, globalObj, funcName, funcObj, kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(funcName);

        // Bind __prismaUpdateWebGLContext as a global function
        JSStringRef updateName = JSStringCreateWithUTF8CString("__prismaUpdateWebGLContext");
        JSObjectRef updateObj = JSObjectMakeFunctionWithCallback(jsCtx, updateName, JS_UpdateWebGLContext);
        JSObjectSetProperty(jsCtx, globalObj, updateName, updateObj, kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(updateName);

        logger::info("[WebGL] WebGL bindings injected into JS context");
    }

}  // namespace PrismaUI::WebGL
