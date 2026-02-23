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

    // Reset per-frame state. Must be called on the Ultralight thread before
    // renderer->Update() each frame so that the first GL call of the frame
    // re-syncs ANGLE's D3D11 state.
    void ResetFrameState();

    // Restore D3D11 render targets that were saved when ANGLE activated.
    // Must be called after renderer->Update() (which runs JS/GL) and before
    // renderer->Render() (which needs Ultralight's D3D11 state).
    void EndFrameGLState();

}  // namespace PrismaUI::WebGL
