#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>

namespace PrismaUI::WebGL {

    struct ANGLEContext {
        EGLDisplay eglDisplay = EGL_NO_DISPLAY;
        EGLContext eglContext = EGL_NO_CONTEXT;
        EGLSurface eglSurface = EGL_NO_SURFACE;
        EGLConfig eglConfig = nullptr;

        // Shared D3D11 texture on Skyrim's device: ReadbackToSharedTexture copies
        // ANGLE's output here; the render thread composites from it via SpriteBatch.
        Microsoft::WRL::ComPtr<ID3D11Texture2D> sharedTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sharedSRV;

        uint32_t canvasWidth = 0;
        uint32_t canvasHeight = 0;

        // Canvas position within the Ultralight view (set by JS shim each frame)
        float canvasX = 0.0f;
        float canvasY = 0.0f;

        // Whether the canvas should be drawn (computed by JS shim)
        bool visible = true;

        // Last update tick from JS (ms since steady_clock epoch)
        uint64_t lastUpdateMs = 0;

        bool initialized = false;
    };

    // Initialize the global ANGLE EGL display with its own D3D11 device.
    // The device parameter is used only for creating the shared compositing texture.
    // Call once after d3dDevice is available.
    bool InitializeANGLEDisplay(ID3D11Device* device);

    // Create a per-canvas ANGLE context with a pbuffer surface.
    ANGLEContext* CreateWebGLContext(uint32_t width, uint32_t height, ID3D11Device* device);

    // Resize the ANGLE surface (recreates pbuffer + shared texture).
    bool ResizeWebGLContext(ANGLEContext* ctx, uint32_t width, uint32_t height, ID3D11Device* device);

    // Make context current on the calling thread.
    bool MakeContextCurrent(ANGLEContext* ctx);

    // Unbind context from the calling thread.
    void MakeContextNotCurrent();

    // Destroy context and free all resources.
    void DestroyWebGLContext(ANGLEContext* ctx);

    // Shut down the global ANGLE display.
    void ShutdownANGLE();

}  // namespace PrismaUI::WebGL
