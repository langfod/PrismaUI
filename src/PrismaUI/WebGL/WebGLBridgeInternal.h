#pragma once

// Internal header for WebGLBridge split files.
// NOT part of the public API — only included by WebGL/*.cpp files.

#include "ANGLEContext.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <JavaScriptCore/JavaScript.h>

#include <cstring>
#include <string>

namespace PrismaUI::WebGL {

    // =========================================================================
    // Frame state
    // =========================================================================
    extern thread_local bool g_contextActivatedThisFrame;

    // =========================================================================
    // Shared helpers
    // =========================================================================
    void EnsureContextActive(ANGLEContext* c);
    ANGLEContext* GetContext(JSObjectRef thisObject);
    GLuint GetGLId(JSContextRef ctx, JSValueRef val);
    JSValueRef MakeGLObject(JSContextRef ctx, const char* className, GLuint id);
    std::string GetString(JSContextRef ctx, JSValueRef val);
    void ReadbackToSharedTexture(ANGLEContext* c);

    // =========================================================================
    // GL callback type alias (matches JSObjectCallAsFunctionCallback)
    // =========================================================================
    using GLCallback = JSValueRef (*)(JSContextRef, JSObjectRef, JSObjectRef,
                                      size_t, const JSValueRef[], JSValueRef*);

    // =========================================================================
    // Context/State
    // =========================================================================
    JSValueRef GL_getError(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_enable(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_disable(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_viewport(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_scissor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_clearColor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_clearDepth(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_clearStencil(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_clear(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_colorMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_depthFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_depthMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_depthRange(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_pixelStorei(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_flush(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_finish(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);

    // =========================================================================
    // Blend
    // =========================================================================
    JSValueRef GL_blendFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_blendFuncSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_blendEquation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_blendEquationSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_blendColor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Culling
    // =========================================================================
    JSValueRef GL_cullFace(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_frontFace(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_lineWidth(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_polygonOffset(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Stencil
    // =========================================================================
    JSValueRef GL_stencilFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_stencilFuncSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_stencilOp(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_stencilOpSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_stencilMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_stencilMaskSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Misc State
    // =========================================================================
    JSValueRef GL_hint(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_sampleCoverage(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Buffers
    // =========================================================================
    JSValueRef GL_createBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bufferData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bufferSubData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Shaders & Programs
    // =========================================================================
    JSValueRef GL_createShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_deleteShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_shaderSource(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_compileShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getShaderParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getShaderInfoLog(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_createProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_attachShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_detachShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_linkProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_useProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getProgramParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getProgramInfoLog(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_validateProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindAttribLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Attributes
    // =========================================================================
    JSValueRef GL_getAttribLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_enableVertexAttribArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_disableVertexAttribArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttribPointer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Uniforms
    // =========================================================================
    JSValueRef GL_getUniformLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform1f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform1i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform2f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform3f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform4f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform2i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform3i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform4i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform1fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform1iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform2iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform3iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform4iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformMatrix2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformMatrix3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformMatrix4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Textures
    // =========================================================================
    JSValueRef GL_createTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_activeTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_texParameteri(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_texParameterf(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_generateMipmap(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_texImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_texSubImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_copyTexImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_copyTexSubImage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_readPixels(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Framebuffers
    // =========================================================================
    JSValueRef GL_createFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_framebufferTexture2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_checkFramebufferStatus(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_framebufferRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Renderbuffers
    // =========================================================================
    JSValueRef GL_createRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_renderbufferStorage(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Drawing
    // =========================================================================
    JSValueRef GL_drawArrays(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_drawElements(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Query / Introspection
    // =========================================================================
    JSValueRef GL_getParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getSupportedExtensions(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_getExtension(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isContextLost(JSContextRef ctx, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_getShaderPrecisionFormat(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getActiveAttrib(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getActiveUniform(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isEnabled(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isRenderbuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isTexture(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isProgram(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isShader(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getBufferParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getRenderbufferParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getTexParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getFramebufferAttachmentParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getContextAttributes(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_getShaderSource(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getAttachedShaders(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getUniform(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getVertexAttrib(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getVertexAttribOffset(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Vertex Attrib Constants
    // =========================================================================
    JSValueRef GL_vertexAttrib1f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttrib2f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttrib3f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttrib4f(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttrib1fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttrib2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttrib3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttrib4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

}  // namespace PrismaUI::WebGL
