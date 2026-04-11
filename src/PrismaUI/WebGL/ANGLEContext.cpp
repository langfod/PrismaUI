#include "ANGLEContext.h"

#include <EGL/eglext_angle.h>
#include <dxgi.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <vector>

// ANGLE EGL extensions for D3D11 interop
#ifndef EGL_ANGLE_platform_angle
    #define EGL_ANGLE_platform_angle 1
    #define EGL_PLATFORM_ANGLE_ANGLE 0x3202
    #define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3203
    #define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#endif

namespace PrismaUI::WebGL {

    // Global ANGLE display (one per process)
    static EGLDisplay g_ANGLEDisplay = EGL_NO_DISPLAY;
    static bool g_ANGLEInitialized = false;

    // Active ANGLE contexts tracked for deferred end-of-frame readback.
    // All access is on the Ultralight thread — no synchronization needed.
    static std::vector<ANGLEContext*> g_activeContexts;

    std::span<ANGLEContext* const> GetActiveContexts() { return g_activeContexts; }

    // Function pointer types for ANGLE EGL extensions
    using PFNEGLGETPLATFORMDISPLAYEXTPROC = EGLDisplay(EGLAPIENTRY*)(EGLenum, void*, const EGLint*);

    bool InitializeANGLEDisplay(ID3D11Device* device) {
        if (g_ANGLEInitialized) {
            return true;
        }

        if (!device) {
            logger::error("[WebGL] InitializeANGLEDisplay: D3D11 device is null");
            return false;
        }

        // Get required extension function pointer
        auto eglGetPlatformDisplayEXT =
            reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));

        if (!eglGetPlatformDisplayEXT) {
            logger::error("[WebGL] Failed to get eglGetPlatformDisplayEXT");
            return false;
        }

        // Let ANGLE create its own D3D11 device instead of sharing Skyrim's.
        // Sharing Skyrim's device causes ANGLE's DrawIndexed calls to silently
        // fail (glClear works but glDrawElements produces zero pixels), likely
        // due to D3D11 state pollution from Ultralight and the game engine.
        // With its own device, ANGLE has full control over D3D11 state.
        const EGLint displayAttribs[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_NONE};

        g_ANGLEDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, displayAttribs);

        if (g_ANGLEDisplay == EGL_NO_DISPLAY) {
            logger::error("[WebGL] eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE) failed: 0x{:X}", eglGetError());
            return false;
        }

        EGLint major = 0, minor = 0;
        if (!eglInitialize(g_ANGLEDisplay, &major, &minor)) {
            logger::error("[WebGL] eglInitialize failed: 0x{:X}", eglGetError());
            g_ANGLEDisplay = EGL_NO_DISPLAY;
            return false;
        }

        logger::info("[WebGL] ANGLE EGL initialized: version {}.{}", major, minor);

        // Check for required extensions
        const char* extensions = eglQueryString(g_ANGLEDisplay, EGL_EXTENSIONS);
        if (extensions) {
            logger::info("[WebGL] EGL extensions: {}", extensions);
        }

        g_ANGLEInitialized = true;
        return true;
    }

    static bool CreateSharedTexture(ANGLEContext* ctx, uint32_t width, uint32_t height, ID3D11Device* device) {
        // Create a D3D11 texture on Skyrim's device for compositing.
        // We try keyed-mutex sharing first (for the zero-copy DXGI path).
        // If that fails, fall back to a plain texture for CPU readback.
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;

        ctx->sharedTexture.Reset();
        ctx->sharedSRV.Reset();
        ctx->skyrimMutex.Reset();
        ctx->sharedHandle = nullptr;

        // Log device info for diagnostics
        D3D_FEATURE_LEVEL featureLevel = device->GetFeatureLevel();
        logger::info("[WebGL] CreateSharedTexture: {}x{}, device feature level: 0x{:X}", width, height,
                     static_cast<uint32_t>(featureLevel));

        // Try keyed-mutex shared texture (needed for zero-copy DXGI path)
        bool sharingReady = false;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, ctx->sharedTexture.GetAddressOf());
        if (SUCCEEDED(hr)) {
            // Get keyed mutex on Skyrim's side
            hr = ctx->sharedTexture.As(&ctx->skyrimMutex);
            if (SUCCEEDED(hr)) {
                // Get DXGI shared handle for cross-device sharing
                Microsoft::WRL::ComPtr<IDXGIResource> dxgiResource;
                hr = ctx->sharedTexture.As(&dxgiResource);
                if (SUCCEEDED(hr)) {
                    hr = dxgiResource->GetSharedHandle(&ctx->sharedHandle);
                    if (SUCCEEDED(hr) && ctx->sharedHandle) {
                        sharingReady = true;
                    }
                }
            }

            if (!sharingReady) {
                logger::warn("[WebGL] Keyed mutex texture created but sharing setup failed (0x{:X})",
                             static_cast<uint32_t>(hr));
                ctx->skyrimMutex.Reset();
                ctx->sharedHandle = nullptr;
                ctx->sharedTexture.Reset();
            }
        } else {
            logger::warn("[WebGL] Keyed mutex texture failed (0x{:X})", static_cast<uint32_t>(hr));
        }

        // If keyed-mutex sharing failed, create a plain texture for CPU readback
        if (!sharingReady) {
            ctx->sharedTexture.Reset();
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            texDesc.MiscFlags = 0;

            hr = device->CreateTexture2D(&texDesc, nullptr, ctx->sharedTexture.GetAddressOf());
            if (FAILED(hr)) {
                logger::error("[WebGL] Failed to create D3D11 texture: 0x{:X}", static_cast<uint32_t>(hr));
                return false;
            }
            logger::info("[WebGL] Created plain D3D11 texture for CPU readback path");
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        hr = device->CreateShaderResourceView(ctx->sharedTexture.Get(), &srvDesc, ctx->sharedSRV.GetAddressOf());
        if (FAILED(hr)) {
            logger::error("[WebGL] Failed to create SRV for shared texture: 0x{:X}", static_cast<uint32_t>(hr));
            ctx->skyrimMutex.Reset();
            ctx->sharedTexture.Reset();
            ctx->sharedHandle = nullptr;
            return false;
        }

        return sharingReady;
    }

    // Try to set up the zero-copy shared texture path:
    // 1. Query ANGLE's internal D3D11 device
    // 2. Open the shared texture on ANGLE's device
    // 3. Create an EGL pbuffer surface backed by the shared texture
    // Returns true if successful; on failure, caller should use CPU readback fallback.
    static bool TrySetupSharedTexturePath(ANGLEContext* ctx) {
        // Get EGL extension function pointers
        auto eglQueryDisplayAttribEXT =
            reinterpret_cast<PFNEGLQUERYDISPLAYATTRIBEXTPROC>(eglGetProcAddress("eglQueryDisplayAttribEXT"));
        auto eglQueryDeviceAttribEXT =
            reinterpret_cast<PFNEGLQUERYDEVICEATTRIBEXTPROC>(eglGetProcAddress("eglQueryDeviceAttribEXT"));

        if (!eglQueryDisplayAttribEXT || !eglQueryDeviceAttribEXT) {
            logger::warn("[WebGL] Shared texture: EGL device query extensions not available");
            return false;
        }

        // Query ANGLE's internal D3D11 device
        EGLDeviceEXT eglDevice = EGL_NO_DEVICE_EXT;
        if (!eglQueryDisplayAttribEXT(ctx->eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice)) ||
            eglDevice == EGL_NO_DEVICE_EXT) {
            logger::warn("[WebGL] Shared texture: failed to query EGL device (err=0x{:X})", eglGetError());
            return false;
        }

        ID3D11Device* angleDevice = nullptr;
        if (!eglQueryDeviceAttribEXT(eglDevice, EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<EGLAttrib*>(&angleDevice)) ||
            !angleDevice) {
            logger::warn("[WebGL] Shared texture: failed to query ANGLE's D3D11 device (err=0x{:X})", eglGetError());
            return false;
        }

        // Open the shared texture on ANGLE's device
        ctx->angleSharedTexture.Reset();
        ctx->angleMutex.Reset();

        HRESULT hr = angleDevice->OpenSharedResource(ctx->sharedHandle, __uuidof(ID3D11Texture2D),
                                                     reinterpret_cast<void**>(ctx->angleSharedTexture.GetAddressOf()));
        if (FAILED(hr)) {
            logger::warn("[WebGL] Shared texture: OpenSharedResource failed: 0x{:X}", static_cast<uint32_t>(hr));
            return false;
        }

        // Get keyed mutex on ANGLE's side
        hr = ctx->angleSharedTexture.As(&ctx->angleMutex);
        if (FAILED(hr)) {
            logger::warn("[WebGL] Shared texture: failed to get ANGLE-side keyed mutex: 0x{:X}",
                         static_cast<uint32_t>(hr));
            ctx->angleSharedTexture.Reset();
            return false;
        }

        // Create EGL pbuffer surface backed by the shared texture.
        // ANGLE's EGL_ANGLE_d3d_texture_client_buffer extension allows creating
        // a surface from an application-provided D3D11 texture.
        const EGLint surfaceAttribs[] = {EGL_WIDTH,
                                         static_cast<EGLint>(ctx->canvasWidth),
                                         EGL_HEIGHT,
                                         static_cast<EGLint>(ctx->canvasHeight),
                                         EGL_TEXTURE_FORMAT,
                                         EGL_TEXTURE_RGBA,
                                         EGL_TEXTURE_TARGET,
                                         EGL_TEXTURE_2D,
                                         EGL_NONE};

        ctx->eglSharedSurface = eglCreatePbufferFromClientBuffer(
            ctx->eglDisplay, EGL_D3D_TEXTURE_ANGLE, ctx->angleSharedTexture.Get(), ctx->eglConfig, surfaceAttribs);

        if (ctx->eglSharedSurface == EGL_NO_SURFACE) {
            logger::warn("[WebGL] Shared texture: eglCreatePbufferFromClientBuffer failed: 0x{:X}", eglGetError());
            ctx->angleMutex.Reset();
            ctx->angleSharedTexture.Reset();
            return false;
        }

        logger::info("[WebGL] Shared texture path established (zero-copy DXGI sharing)");
        return true;
    }

    // Tear down shared texture path resources (ANGLE side only).
    // Called during resize and destroy.
    static void TeardownSharedTexturePath(ANGLEContext* ctx) {
        if (ctx->eglSharedSurface != EGL_NO_SURFACE) {
            eglDestroySurface(ctx->eglDisplay, ctx->eglSharedSurface);
            ctx->eglSharedSurface = EGL_NO_SURFACE;
        }
        ctx->angleMutex.Reset();
        ctx->angleSharedTexture.Reset();
        ctx->useSharedTexturePath = false;
    }

    ANGLEContext* CreateWebGLContext(uint32_t width, uint32_t height, ID3D11Device* device) {
        if (!g_ANGLEInitialized || g_ANGLEDisplay == EGL_NO_DISPLAY) {
            logger::error("[WebGL] CreateWebGLContext: ANGLE not initialized");
            return nullptr;
        }

        if (!device || width == 0 || height == 0) {
            logger::error("[WebGL] CreateWebGLContext: invalid parameters (device={}, {}x{})",
                          static_cast<void*>(device), width, height);
            return nullptr;
        }

        auto* ctx = new ANGLEContext();
        ctx->eglDisplay = g_ANGLEDisplay;
        ctx->canvasWidth = width;
        ctx->canvasHeight = height;

        // Choose an EGL config
        const EGLint configAttribs[] = {EGL_RED_SIZE,
                                        8,
                                        EGL_GREEN_SIZE,
                                        8,
                                        EGL_BLUE_SIZE,
                                        8,
                                        EGL_ALPHA_SIZE,
                                        8,
                                        EGL_DEPTH_SIZE,
                                        24,
                                        EGL_STENCIL_SIZE,
                                        8,
                                        EGL_RENDERABLE_TYPE,
                                        EGL_OPENGL_ES3_BIT,
                                        EGL_SURFACE_TYPE,
                                        EGL_PBUFFER_BIT,
                                        EGL_NONE};

        EGLint numConfigs = 0;
        if (!eglChooseConfig(g_ANGLEDisplay, configAttribs, &ctx->eglConfig, 1, &numConfigs) || numConfigs == 0) {
            logger::error("[WebGL] eglChooseConfig failed: 0x{:X}", eglGetError());
            delete ctx;
            return nullptr;
        }

        // Create the shared D3D11 texture on Skyrim's device for compositing.
        // If this fails, we'll fall through to the CPU readback path below.
        bool sharedTextureReady = CreateSharedTexture(ctx, width, height, device);

        // Create a regular pbuffer surface on ANGLE's own D3D11 device.
        // This is used as the render target when the shared texture path
        // is unavailable (CPU readback fallback).
        const EGLint surfaceAttribs[] = {EGL_WIDTH, static_cast<EGLint>(width), EGL_HEIGHT, static_cast<EGLint>(height),
                                         EGL_NONE};

        ctx->eglSurface = eglCreatePbufferSurface(g_ANGLEDisplay, ctx->eglConfig, surfaceAttribs);
        if (ctx->eglSurface == EGL_NO_SURFACE) {
            logger::error("[WebGL] eglCreatePbufferSurface failed: 0x{:X}", eglGetError());
            delete ctx;
            return nullptr;
        }

        // Create an OpenGL ES 3.0 context
        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

        ctx->eglContext = eglCreateContext(g_ANGLEDisplay, ctx->eglConfig, EGL_NO_CONTEXT, contextAttribs);
        if (ctx->eglContext == EGL_NO_CONTEXT) {
            logger::error("[WebGL] eglCreateContext failed: 0x{:X}", eglGetError());
            eglDestroySurface(g_ANGLEDisplay, ctx->eglSurface);
            delete ctx;
            return nullptr;
        }

        // Make the context current on the calling thread (ultralight thread)
        if (!eglMakeCurrent(g_ANGLEDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext)) {
            logger::error("[WebGL] eglMakeCurrent failed: 0x{:X}", eglGetError());
            eglDestroyContext(g_ANGLEDisplay, ctx->eglContext);
            eglDestroySurface(g_ANGLEDisplay, ctx->eglSurface);
            delete ctx;
            return nullptr;
        }

        ctx->initialized = true;

        g_activeContexts.push_back(ctx);

        // Try to set up the zero-copy shared texture path.
        // If successful, ANGLE renders directly into the DXGI shared texture
        // and we switch eglSurface to the shared surface.
        if (sharedTextureReady && TrySetupSharedTexturePath(ctx)) {
            ctx->useSharedTexturePath = true;
            // Switch to the shared surface for rendering
            EGLSurface oldSurface = ctx->eglSurface;
            ctx->eglSurface = ctx->eglSharedSurface;
            if (eglMakeCurrent(g_ANGLEDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext)) {
                eglDestroySurface(g_ANGLEDisplay, oldSurface);
                logger::info("[WebGL] Using zero-copy DXGI shared texture path");
            } else {
                logger::error("[WebGL] eglMakeCurrent failed switching to shared surface, reverting");
                ctx->eglSurface = oldSurface;
                eglMakeCurrent(g_ANGLEDisplay, oldSurface, oldSurface, ctx->eglContext);
                TeardownSharedTexturePath(ctx);
                ctx->useSharedTexturePath = false;
            }
        }

        if (!ctx->useSharedTexturePath) {
            // Fallback: allocate readback buffers for CPU copy path
            size_t bufSize = static_cast<size_t>(width) * height * 4;
            ctx->readbackPixels.resize(bufSize);
            ctx->readbackFlipped.resize(bufSize);
            logger::info("[WebGL] Using CPU readback path (shared texture unavailable)");
        }

        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* extensions = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
        logger::info("[WebGL] Created WebGL context: {}x{}, GL_RENDERER={}", width, height,
                     renderer ? renderer : "<unknown>");
        logger::info("[WebGL] GL_VERSION={}", version ? version : "<unknown>");
        logger::info("[WebGL] GL_EXTENSIONS={}", extensions ? extensions : "<unknown>");

        return ctx;
    }

    bool ResizeWebGLContext(ANGLEContext* ctx, uint32_t width, uint32_t height, ID3D11Device* device) {
        if (!ctx || !ctx->initialized || width == 0 || height == 0) {
            return false;
        }

        if (ctx->canvasWidth == width && ctx->canvasHeight == height) {
            return true;  // No change needed
        }

        // Unbind current context
        eglMakeCurrent(g_ANGLEDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        // Tear down old shared texture path (if active)
        bool wasShared = ctx->useSharedTexturePath;
        TeardownSharedTexturePath(ctx);

        // Destroy old surface (only if not the shared surface — already destroyed above)
        if (!wasShared && ctx->eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(g_ANGLEDisplay, ctx->eglSurface);
        }
        ctx->eglSurface = EGL_NO_SURFACE;

        // Create new shared texture at new size (non-fatal if it fails)
        bool sharedTextureReady = CreateSharedTexture(ctx, width, height, device);

        // Create new pbuffer surface on ANGLE's own device (fallback target)
        const EGLint surfaceAttribs[] = {EGL_WIDTH, static_cast<EGLint>(width), EGL_HEIGHT, static_cast<EGLint>(height),
                                         EGL_NONE};

        ctx->eglSurface = eglCreatePbufferSurface(g_ANGLEDisplay, ctx->eglConfig, surfaceAttribs);
        if (ctx->eglSurface == EGL_NO_SURFACE) {
            logger::error("[WebGL] ResizeWebGLContext: eglCreatePbufferSurface failed: 0x{:X}", eglGetError());
            return false;
        }

        ctx->canvasWidth = width;
        ctx->canvasHeight = height;

        // Try shared texture path again at new size
        if (sharedTextureReady && TrySetupSharedTexturePath(ctx)) {
            ctx->useSharedTexturePath = true;
            EGLSurface oldSurface = ctx->eglSurface;
            ctx->eglSurface = ctx->eglSharedSurface;
            if (eglMakeCurrent(g_ANGLEDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext)) {
                eglDestroySurface(g_ANGLEDisplay, oldSurface);
                // Shrink readback buffers since they're not needed
                ctx->readbackPixels.clear();
                ctx->readbackPixels.shrink_to_fit();
                ctx->readbackFlipped.clear();
                ctx->readbackFlipped.shrink_to_fit();
            } else {
                logger::error(
                    "[WebGL] ResizeWebGLContext: eglMakeCurrent failed switching to shared surface, reverting");
                ctx->eglSurface = oldSurface;
                eglMakeCurrent(g_ANGLEDisplay, oldSurface, oldSurface, ctx->eglContext);
                TeardownSharedTexturePath(ctx);
                ctx->useSharedTexturePath = false;
            }
        }

        if (!ctx->useSharedTexturePath) {
            // Rebind context with regular pbuffer
            if (!eglMakeCurrent(g_ANGLEDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext)) {
                logger::error("[WebGL] ResizeWebGLContext: eglMakeCurrent failed: 0x{:X}", eglGetError());
                return false;
            }
            size_t bufSize = static_cast<size_t>(width) * height * 4;
            ctx->readbackPixels.resize(bufSize);
            ctx->readbackFlipped.resize(bufSize);
        }

        logger::info("[WebGL] Resized WebGL context to {}x{} (shared={})", width, height, ctx->useSharedTexturePath);
        return true;
    }

    bool MakeContextCurrent(ANGLEContext* ctx) {
        if (!ctx || !ctx->initialized) {
            return false;
        }
        return eglMakeCurrent(g_ANGLEDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext) == EGL_TRUE;
    }

    void MakeContextNotCurrent() {
        if (g_ANGLEDisplay != EGL_NO_DISPLAY) {
            eglMakeCurrent(g_ANGLEDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
    }

    void DestroyWebGLContext(ANGLEContext* ctx) {
        if (!ctx) {
            return;
        }

        g_activeContexts.erase(std::remove(g_activeContexts.begin(), g_activeContexts.end(), ctx),
                               g_activeContexts.end());

        if (g_ANGLEDisplay != EGL_NO_DISPLAY) {
            eglMakeCurrent(g_ANGLEDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (ctx->eglContext != EGL_NO_CONTEXT) {
                eglDestroyContext(g_ANGLEDisplay, ctx->eglContext);
            }

            // If using shared path, eglSurface == eglSharedSurface — destroy once.
            // If using fallback path, eglSurface is the regular pbuffer.
            bool surfaceIsShared =
                (ctx->eglSurface == ctx->eglSharedSurface && ctx->eglSharedSurface != EGL_NO_SURFACE);

            // Tear down shared texture path (destroys eglSharedSurface)
            TeardownSharedTexturePath(ctx);

            // Destroy the regular pbuffer only if it's separate from the shared surface
            if (!surfaceIsShared && ctx->eglSurface != EGL_NO_SURFACE) {
                eglDestroySurface(g_ANGLEDisplay, ctx->eglSurface);
            }
        }

        ctx->skyrimMutex.Reset();
        ctx->sharedTexture.Reset();
        ctx->sharedSRV.Reset();
        ctx->sharedHandle = nullptr;
        ctx->initialized = false;

        logger::info("[WebGL] Destroyed WebGL context");
        delete ctx;
    }

    void ShutdownANGLE() {
        if (g_ANGLEDisplay != EGL_NO_DISPLAY) {
            if (!g_activeContexts.empty()) {
                logger::warn("[WebGL] ShutdownANGLE: {} contexts still active, cleaning up", g_activeContexts.size());
                // Copy the vector: DestroyWebGLContext modifies g_activeContexts
                auto remaining = g_activeContexts;
                for (auto* ctx : remaining) {
                    DestroyWebGLContext(ctx);
                }
                g_activeContexts.clear();
            }
            eglTerminate(g_ANGLEDisplay);
            g_ANGLEDisplay = EGL_NO_DISPLAY;
        }
        g_ANGLEInitialized = false;
        logger::info("[WebGL] ANGLE shut down");
    }

}  // namespace PrismaUI::WebGL
