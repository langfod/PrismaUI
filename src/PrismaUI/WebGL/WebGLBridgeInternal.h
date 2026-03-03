#pragma once

// Internal header for WebGLBridge split files.
// NOT part of the public API — only included by WebGL/*.cpp files.

#include "ANGLEContext.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
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

    // Helper to extract a float/int/uint array from either a TypedArray or a plain JS Array.
    // Returns the count of elements extracted.  Writes into 'out' up to 'maxCount' elements.
    // If the source is a TypedArray, sets *directPtr to the raw pointer and returns the
    // element count (caller can use the pointer directly without copying).
    // If the source is a plain Array, copies values into 'out' and sets *directPtr = nullptr.
    template <typename T>
    size_t ExtractNumericArray(JSContextRef ctx, JSValueRef val,
                               T* out, size_t maxCount, const T** directPtr);

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
    // WebGL2: Uint uniforms
    JSValueRef GL_uniform1ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform2ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform3ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform4ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform1uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform2uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform3uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniform4uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    // WebGL2: Non-square matrix uniforms
    JSValueRef GL_uniformMatrix2x3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformMatrix3x2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformMatrix2x4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformMatrix4x2fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformMatrix3x4fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformMatrix4x3fv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

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
    // WebGL2: 3D Textures & Storage
    JSValueRef GL_texStorage2D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_texStorage3D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_texImage3D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_texSubImage3D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_copyTexSubImage3D(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

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
    JSValueRef GL_drawArraysInstanced(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_drawElementsInstanced(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_drawRangeElements(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttribDivisor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Vertex Array Objects (WebGL2)
    // =========================================================================
    JSValueRef GL_createVertexArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteVertexArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindVertexArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isVertexArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Uniform Buffer Objects (WebGL2)
    // =========================================================================
    JSValueRef GL_bindBufferBase(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindBufferRange(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_uniformBlockBinding(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getUniformBlockIndex(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getActiveUniformBlockName(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getActiveUniformBlockParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getUniformIndices(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getActiveUniforms(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Transform Feedback (WebGL2)
    // =========================================================================
    JSValueRef GL_createTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_beginTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_endTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_pauseTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_resumeTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_transformFeedbackVaryings(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getTransformFeedbackVarying(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

    // =========================================================================
    // Framebuffer Enhancements (WebGL2)
    // =========================================================================
    JSValueRef GL_drawBuffers(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_readBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_blitFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_framebufferTextureLayer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_renderbufferStorageMultisample(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_invalidateFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_invalidateSubFramebuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

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
    // WebGL2: Integer vertex attribs
    JSValueRef GL_vertexAttribIPointer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttribI4i(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttribI4ui(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttribI4iv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_vertexAttribI4uiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    // WebGL2: Clear Buffer
    JSValueRef GL_clearBufferiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_clearBufferuiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_clearBufferfv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_clearBufferfi(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    // WebGL2: Samplers
    JSValueRef GL_createSampler(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteSampler(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_bindSampler(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isSampler(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_samplerParameteri(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_samplerParameterf(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getSamplerParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    // WebGL2: Queries
    JSValueRef GL_createQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*);
    JSValueRef GL_deleteQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_beginQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_endQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getQueryParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    // WebGL2: Sync
    JSValueRef GL_fenceSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_isSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_deleteSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_clientWaitSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_waitSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getSyncParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    // WebGL2: Buffer Operations
    JSValueRef GL_copyBufferSubData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getBufferSubData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    // WebGL2: Misc Queries
    JSValueRef GL_getFragDataLocation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);
    JSValueRef GL_getIndexedParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject, size_t argc, const JSValueRef argv[], JSValueRef*);

}  // namespace PrismaUI::WebGL
