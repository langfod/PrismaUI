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
        {"detachShader",                GL_detachShader,                kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
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
        {"uniform2i",                   GL_uniform2i,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform2iv",                  GL_uniform2iv,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform3i",                   GL_uniform3i,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform3iv",                  GL_uniform3iv,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform4i",                   GL_uniform4i,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"uniform4iv",                  GL_uniform4iv,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
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
        {"copyTexImage2D",              GL_copyTexImage2D,              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"copyTexSubImage2D",           GL_copyTexSubImage2D,           kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
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
        {"stencilFuncSeparate",         GL_stencilFuncSeparate,         kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"stencilOp",                   GL_stencilOp,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"stencilOpSeparate",           GL_stencilOpSeparate,           kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"stencilMask",                 GL_stencilMask,                 kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"stencilMaskSeparate",         GL_stencilMaskSeparate,         kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Misc State
        {"hint",                        GL_hint,                        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"sampleCoverage",              GL_sampleCoverage,              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Reading
        {"readPixels",                  GL_readPixels,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Vertex Attrib Constants
        {"vertexAttrib1f",              GL_vertexAttrib1f,              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"vertexAttrib2f",              GL_vertexAttrib2f,              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"vertexAttrib3f",              GL_vertexAttrib3f,              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"vertexAttrib4f",              GL_vertexAttrib4f,              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"vertexAttrib1fv",             GL_vertexAttrib1fv,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"vertexAttrib2fv",             GL_vertexAttrib2fv,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"vertexAttrib3fv",             GL_vertexAttrib3fv,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"vertexAttrib4fv",             GL_vertexAttrib4fv,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // Boolean queries
        {"isEnabled",                   GL_isEnabled,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"isBuffer",                    GL_isBuffer,                    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"isFramebuffer",               GL_isFramebuffer,               kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"isRenderbuffer",              GL_isRenderbuffer,              kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"isTexture",                   GL_isTexture,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"isProgram",                   GL_isProgram,                   kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"isShader",                    GL_isShader,                    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        // State queries
        {"getBufferParameter",          GL_getBufferParameter,          kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getRenderbufferParameter",    GL_getRenderbufferParameter,    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getTexParameter",             GL_getTexParameter,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getFramebufferAttachmentParameter", GL_getFramebufferAttachmentParameter, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getContextAttributes",        GL_getContextAttributes,        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getShaderSource",             GL_getShaderSource,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getAttachedShaders",          GL_getAttachedShaders,          kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getUniform",                  GL_getUniform,                  kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getVertexAttrib",             GL_getVertexAttrib,             kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"getVertexAttribOffset",       GL_getVertexAttribOffset,       kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
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

        angleCtx->canvasX = canvasX;
        angleCtx->canvasY = canvasY;
        angleCtx->visible = visible;
        angleCtx->lastUpdateMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

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
