#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace PrismaUI::WebGL {

    struct ANGLEContext {
        EGLDisplay eglDisplay = EGL_NO_DISPLAY;
        EGLContext eglContext = EGL_NO_CONTEXT;
        EGLSurface eglSurface = EGL_NO_SURFACE;
        EGLConfig eglConfig = nullptr;

        // Shared D3D11 texture on Skyrim's device for compositing.
        // When useSharedTexturePath is true, ANGLE renders directly into this
        // texture via a DXGI shared handle (zero-copy).  Otherwise,
        // ReadbackToSharedTexture copies pixels here via glReadPixels.
        Microsoft::WRL::ComPtr<ID3D11Texture2D> sharedTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sharedSRV;

        // DXGI shared texture path (zero-copy cross-device rendering)
        bool useSharedTexturePath = false;
        HANDLE sharedHandle = nullptr;
        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> skyrimMutex;         // Skyrim's side of the keyed mutex
        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> angleMutex;          // ANGLE's side of the keyed mutex
        Microsoft::WRL::ComPtr<ID3D11Texture2D> angleSharedTexture;  // Shared texture opened on ANGLE's device
        EGLSurface eglSharedSurface = EGL_NO_SURFACE;                // EGL pbuffer backed by shared texture
        bool mutexReleasedThisFrame = false;  // Set by FlushDirtyContexts when ANGLE releases mutex
        bool angleMutexAcquired = false;      // Whether ANGLE currently holds the keyed mutex

        uint32_t canvasWidth = 0;
        uint32_t canvasHeight = 0;

        // CSS display size of the canvas (may differ from buffer size when
        // CSS scaling / object-fit is used).  Used for SpriteBatch overlay sizing.
        // Written on JS thread, read on render thread — must be atomic.
        std::atomic<float> displayWidth{0.0f};
        std::atomic<float> displayHeight{0.0f};

        // Canvas position within the Ultralight view (set by JS shim each frame)
        // Written on JS thread, read on render thread — must be atomic.
        std::atomic<float> canvasX{0.0f};
        std::atomic<float> canvasY{0.0f};

        // Whether the canvas should be drawn (computed by JS shim)
        // Written on JS thread, read on render thread — must be atomic.
        std::atomic<bool> visible{true};

        // Last update tick from JS (ms since steady_clock epoch)
        // Written on JS thread, read on render thread — must be atomic.
        std::atomic<uint64_t> lastUpdateMs{0};

        // Set by draw calls; cleared after end-of-frame readback
        bool frameDirty = false;

        // Persistent readback buffers (reused across frames, resized on canvas resize)
        std::vector<GLubyte> readbackPixels;
        std::vector<GLubyte> readbackFlipped;

        bool initialized = false;

        // WebGL2 sync objects: maps uint32_t id then GLsync pointer.
        // GLsync is a 64-bit pointer; we never cast it through GLuint.
        std::unordered_map<uint32_t, GLsync> syncObjects;
        std::atomic<uint32_t> nextSyncId{1};
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

    // Get all active ANGLE contexts (for deferred end-of-frame readback).
    std::span<ANGLEContext* const> GetActiveContexts();

}  // namespace PrismaUI::WebGL
