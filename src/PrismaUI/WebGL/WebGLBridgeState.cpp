#include "WebGLBridgeInternal.h"

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

}  // namespace PrismaUI::WebGL
