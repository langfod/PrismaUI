#pragma once

#include <JavaScriptCore/JavaScript.h>

#include <cstdint>

namespace PrismaUI::WebGL {

    struct ANGLEContext;

    // Initialize WebGL JSC bindings for a view. Injects the __prismaCreateWebGLContext
    // native function onto the global JS object. Called from OnWindowObjectReady.
    void InjectWebGLBindings(JSContextRef jsCtx, uint64_t viewId);

    // Get the JSClassRef for WebGLRenderingContext (created once, shared)
    JSClassRef GetWebGLContextClass();

}  // namespace PrismaUI::WebGL
