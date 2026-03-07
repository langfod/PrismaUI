#include "WebGLBridgeInternal.h"

#include <vector>

namespace PrismaUI::WebGL {

    // =========================================================================
    // Context/State
    // =========================================================================

    JSValueRef GL_getError(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                           size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, static_cast<double>(glGetError()));
    }

    JSValueRef GL_enable(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                         size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glEnable(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_disable(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                          size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glDisable(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_viewport(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_scissor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_clearColor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_clearDepth(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glClearDepthf(static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_clearStencil(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glClearStencil(static_cast<GLint>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_clear(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glClear(static_cast<GLbitfield>(JSValueToNumber(ctx, argv[0], nullptr)));
        c->frameDirty = true;
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_colorMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_depthFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glDepthFunc(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_depthMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glDepthMask(JSValueToBoolean(ctx, argv[0]));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_depthRange(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glDepthRangef(
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_pixelStorei(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_flush(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                        size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeUndefined(ctx);
        glFlush();
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_finish(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                         size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeUndefined(ctx);
        glFinish();
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Blend
    // =========================================================================

    JSValueRef GL_blendFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glBlendFunc(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_blendFuncSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    JSValueRef GL_blendEquation(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glBlendEquation(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_blendEquationSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glBlendEquationSeparate(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_blendColor(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
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

    // =========================================================================
    // Culling
    // =========================================================================

    JSValueRef GL_cullFace(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glCullFace(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_frontFace(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glFrontFace(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_lineWidth(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glLineWidth(static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_polygonOffset(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glPolygonOffset(
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Stencil
    // =========================================================================

    JSValueRef GL_stencilFunc(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glStencilFunc(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_stencilFuncSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glStencilFuncSeparate(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_stencilOp(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        glStencilOp(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_stencilOpSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glStencilOpSeparate(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[3], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_stencilMask(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glStencilMask(static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_stencilMaskSeparate(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                      size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glStencilMaskSeparate(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Misc State
    // =========================================================================

    JSValueRef GL_hint(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glHint(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_sampleCoverage(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        glSampleCoverage(
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[0], nullptr)),
            JSValueToBoolean(ctx, argv[1]));
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Clear Buffer (WebGL2)
    // =========================================================================

    JSValueRef GL_clearBufferiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLenum buffer = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint drawbuffer = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        if (JSValueIsObject(ctx, argv[2])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[2], nullptr);
            auto* ptr = static_cast<const GLint*>(GetTypedArrayDataPtr(ctx, arr));
            if (ptr) {
                glClearBufferiv(buffer, drawbuffer, ptr);
                c->frameDirty = true;
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_clearBufferuiv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLenum buffer = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint drawbuffer = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        if (JSValueIsObject(ctx, argv[2])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[2], nullptr);
            auto* ptr = static_cast<const GLuint*>(GetTypedArrayDataPtr(ctx, arr));
            if (ptr) {
                glClearBufferuiv(buffer, drawbuffer, ptr);
                c->frameDirty = true;
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_clearBufferfv(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLenum buffer = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLint drawbuffer = static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr));
        if (JSValueIsObject(ctx, argv[2])) {
            JSObjectRef arr = JSValueToObject(ctx, argv[2], nullptr);
            auto* ptr = static_cast<const GLfloat*>(GetTypedArrayDataPtr(ctx, arr));
            if (ptr) {
                glClearBufferfv(buffer, drawbuffer, ptr);
                c->frameDirty = true;
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_clearBufferfi(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 4) return JSValueMakeUndefined(ctx);
        glClearBufferfi(
            static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[1], nullptr)),
            static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr)),
            static_cast<GLint>(JSValueToNumber(ctx, argv[3], nullptr)));
        c->frameDirty = true;
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Uniform Buffer Objects — binding (WebGL2)
    // =========================================================================

    JSValueRef GL_bindBufferBase(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLuint buffer = 0;
        if (!JSValueIsNull(ctx, argv[2]) && !JSValueIsUndefined(ctx, argv[2]))
            buffer = GetGLId(ctx, argv[2]);
        glBindBufferBase(target, index, buffer);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindBufferRange(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 5) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLuint buffer = 0;
        if (!JSValueIsNull(ctx, argv[2]) && !JSValueIsUndefined(ctx, argv[2]))
            buffer = GetGLId(ctx, argv[2]);
        GLintptr offset = static_cast<GLintptr>(JSValueToNumber(ctx, argv[3], nullptr));
        GLsizeiptr size = static_cast<GLsizeiptr>(JSValueToNumber(ctx, argv[4], nullptr));
        glBindBufferRange(target, index, buffer, offset, size);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_uniformBlockBinding(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLuint program = GetGLId(ctx, argv[0]);
        GLuint blockIndex = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));
        GLuint blockBinding = static_cast<GLuint>(JSValueToNumber(ctx, argv[2], nullptr));
        glUniformBlockBinding(program, blockIndex, blockBinding);
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Vertex Array Objects (WebGL2)
    // =========================================================================

    JSValueRef GL_createVertexArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        return MakeGLObject(ctx, "WebGLVertexArrayObject", vao);
    }

    JSValueRef GL_deleteVertexArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint vao = GetGLId(ctx, argv[0]);
        if (vao) glDeleteVertexArrays(1, &vao);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindVertexArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                   size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint vao = 0;
        if (!JSValueIsNull(ctx, argv[0]) && !JSValueIsUndefined(ctx, argv[0])) {
            vao = GetGLId(ctx, argv[0]);
        }
        glBindVertexArray(vao);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_isVertexArray(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        GLuint vao = GetGLId(ctx, argv[0]);
        return JSValueMakeBoolean(ctx, glIsVertexArray(vao) == GL_TRUE);
    }

    // =========================================================================
    // Transform Feedback (WebGL2)
    // =========================================================================

    JSValueRef GL_createTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint tf = 0;
        glGenTransformFeedbacks(1, &tf);
        return MakeGLObject(ctx, "WebGLTransformFeedback", tf);
    }

    JSValueRef GL_deleteTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint tf = GetGLId(ctx, argv[0]);
        if (tf) glDeleteTransformFeedbacks(1, &tf);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                         size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint tf = 0;
        if (!JSValueIsNull(ctx, argv[1]) && !JSValueIsUndefined(ctx, argv[1]))
            tf = GetGLId(ctx, argv[1]);
        glBindTransformFeedback(target, tf);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_isTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        GLuint tf = GetGLId(ctx, argv[0]);
        return JSValueMakeBoolean(ctx, glIsTransformFeedback(tf) == GL_TRUE);
    }

    JSValueRef GL_beginTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                          size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glBeginTransformFeedback(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_endTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                        size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeUndefined(ctx);
        glEndTransformFeedback();
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_pauseTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                          size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeUndefined(ctx);
        glPauseTransformFeedback();
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_resumeTransformFeedback(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                           size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeUndefined(ctx);
        glResumeTransformFeedback();
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_transformFeedbackVaryings(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLuint program = GetGLId(ctx, argv[0]);

        JSObjectRef arr = JSValueToObject(ctx, argv[1], nullptr);
        if (!arr) return JSValueMakeUndefined(ctx);

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

        GLenum bufferMode = static_cast<GLenum>(JSValueToNumber(ctx, argv[2], nullptr));
        glTransformFeedbackVaryings(program, count, namesPtrs.data(), bufferMode);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_getTransformFeedbackVarying(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint program = GetGLId(ctx, argv[0]);
        GLuint index = static_cast<GLuint>(JSValueToNumber(ctx, argv[1], nullptr));

        GLint maxLen = 0;
        glGetProgramiv(program, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &maxLen);
        if (maxLen < 1) maxLen = 1;
        std::vector<GLchar> name(maxLen);
        GLsizei len = 0;
        GLsizei size = 0;
        GLenum type = 0;
        glGetTransformFeedbackVarying(program, index, maxLen, &len, &size, &type, name.data());

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

    // =========================================================================
    // Samplers (WebGL2)
    // =========================================================================

    JSValueRef GL_createSampler(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint sampler = 0;
        glGenSamplers(1, &sampler);
        return MakeGLObject(ctx, "WebGLSampler", sampler);
    }

    JSValueRef GL_deleteSampler(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                 size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint sampler = GetGLId(ctx, argv[0]);
        if (sampler) glDeleteSamplers(1, &sampler);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_bindSampler(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLuint unit = static_cast<GLuint>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint sampler = 0;
        if (!JSValueIsNull(ctx, argv[1]) && !JSValueIsUndefined(ctx, argv[1]))
            sampler = GetGLId(ctx, argv[1]);
        glBindSampler(unit, sampler);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_isSampler(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        GLuint sampler = GetGLId(ctx, argv[0]);
        return JSValueMakeBoolean(ctx, glIsSampler(sampler) == GL_TRUE);
    }

    JSValueRef GL_samplerParameteri(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLuint sampler = GetGLId(ctx, argv[0]);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint param = static_cast<GLint>(JSValueToNumber(ctx, argv[2], nullptr));
        glSamplerParameteri(sampler, pname, param);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_samplerParameterf(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLuint sampler = GetGLId(ctx, argv[0]);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLfloat param = static_cast<GLfloat>(JSValueToNumber(ctx, argv[2], nullptr));
        glSamplerParameterf(sampler, pname, param);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_getSamplerParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                       size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint sampler = GetGLId(ctx, argv[0]);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));

        switch (pname) {
            case GL_TEXTURE_MIN_LOD:
            case GL_TEXTURE_MAX_LOD: {
                GLfloat val = 0;
                glGetSamplerParameterfv(sampler, pname, &val);
                return JSValueMakeNumber(ctx, val);
            }
            default: {
                GLint val = 0;
                glGetSamplerParameteriv(sampler, pname, &val);
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
        }
    }

    // =========================================================================
    // Queries (WebGL2)
    // =========================================================================

    JSValueRef GL_createQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t, const JSValueRef[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized) return JSValueMakeNull(ctx);
        GLuint query = 0;
        glGenQueries(1, &query);
        return MakeGLObject(ctx, "WebGLQuery", query);
    }

    JSValueRef GL_deleteQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                               size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint query = GetGLId(ctx, argv[0]);
        if (query) glDeleteQueries(1, &query);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_isQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                           size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        GLuint query = GetGLId(ctx, argv[0]);
        return JSValueMakeBoolean(ctx, glIsQuery(query) == GL_TRUE);
    }

    JSValueRef GL_beginQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeUndefined(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLuint query = GetGLId(ctx, argv[1]);
        glBeginQuery(target, query);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_endQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        glEndQuery(static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr)));
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_getQuery(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLenum target = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint val = 0;
        glGetQueryiv(target, pname, &val);
        if (pname == GL_CURRENT_QUERY) {
            if (val == 0) return JSValueMakeNull(ctx);
            return MakeGLObject(ctx, "WebGLQuery", static_cast<GLuint>(val));
        }
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

    JSValueRef GL_getQueryParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                     size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint query = GetGLId(ctx, argv[0]);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));

        if (pname == GL_QUERY_RESULT_AVAILABLE) {
            GLuint val = 0;
            glGetQueryObjectuiv(query, pname, &val);
            return JSValueMakeBoolean(ctx, val != 0);
        }
        GLuint val = 0;
        glGetQueryObjectuiv(query, pname, &val);
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

    // =========================================================================
    // Sync (WebGL2)
    // =========================================================================

    JSValueRef GL_fenceSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                             size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLenum condition = static_cast<GLenum>(JSValueToNumber(ctx, argv[0], nullptr));
        GLbitfield flags = static_cast<GLbitfield>(JSValueToNumber(ctx, argv[1], nullptr));
        GLsync sync = glFenceSync(condition, flags);
        if (!sync) return JSValueMakeNull(ctx);
        uint32_t id = c->nextSyncId.fetch_add(1);
        c->syncObjects[id] = sync;
        return MakeGLObject(ctx, "WebGLSync", id);
    }

    JSValueRef GL_isSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                          size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeBoolean(ctx, false);
        GLuint id = GetGLId(ctx, argv[0]);
        auto it = c->syncObjects.find(id);
        if (it == c->syncObjects.end()) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, glIsSync(it->second) == GL_TRUE);
    }

    JSValueRef GL_deleteSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                              size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 1) return JSValueMakeUndefined(ctx);
        GLuint id = GetGLId(ctx, argv[0]);
        if (id) {
            auto it = c->syncObjects.find(id);
            if (it != c->syncObjects.end()) {
                glDeleteSync(it->second);
                c->syncObjects.erase(it);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_clientWaitSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                  size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeNumber(ctx, GL_WAIT_FAILED);
        GLuint id = GetGLId(ctx, argv[0]);
        auto syncIt = c->syncObjects.find(id);
        if (syncIt == c->syncObjects.end()) return JSValueMakeNumber(ctx, GL_WAIT_FAILED);
        GLbitfield flags = static_cast<GLbitfield>(JSValueToNumber(ctx, argv[1], nullptr));
        GLuint64 timeout = static_cast<GLuint64>(JSValueToNumber(ctx, argv[2], nullptr));
        GLenum result = glClientWaitSync(syncIt->second, flags, timeout);
        return JSValueMakeNumber(ctx, static_cast<double>(result));
    }

    JSValueRef GL_waitSync(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                            size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 3) return JSValueMakeUndefined(ctx);
        GLuint id = GetGLId(ctx, argv[0]);
        auto syncIt = c->syncObjects.find(id);
        if (syncIt == c->syncObjects.end()) return JSValueMakeUndefined(ctx);
        GLbitfield flags = static_cast<GLbitfield>(JSValueToNumber(ctx, argv[1], nullptr));
        GLuint64 timeout = static_cast<GLuint64>(JSValueToNumber(ctx, argv[2], nullptr));
        glWaitSync(syncIt->second, flags, timeout);
        return JSValueMakeUndefined(ctx);
    }

    JSValueRef GL_getSyncParameter(JSContextRef ctx, JSObjectRef, JSObjectRef thisObject,
                                    size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* c = GetContext(thisObject);
        if (!c || !c->initialized || argc < 2) return JSValueMakeNull(ctx);
        GLuint id = GetGLId(ctx, argv[0]);
        auto syncIt = c->syncObjects.find(id);
        if (syncIt == c->syncObjects.end()) return JSValueMakeNull(ctx);
        GLenum pname = static_cast<GLenum>(JSValueToNumber(ctx, argv[1], nullptr));
        GLint val = 0;
        GLsizei len = 0;
        glGetSynciv(syncIt->second, pname, 1, &len, &val);
        return JSValueMakeNumber(ctx, static_cast<double>(val));
    }

}  // namespace PrismaUI::WebGL
