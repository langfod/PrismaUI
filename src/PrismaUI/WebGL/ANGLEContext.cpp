#include "ANGLEContext.h"

#include <spdlog/spdlog.h>

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
        const EGLint displayAttribs[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
            EGL_NONE};

        g_ANGLEDisplay = eglGetPlatformDisplayEXT(
            EGL_PLATFORM_ANGLE_ANGLE,
            EGL_DEFAULT_DISPLAY,
            displayAttribs);

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
        // Create D3D11 texture that will be shared between ANGLE and the render thread
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        ctx->sharedTexture.Reset();
        ctx->sharedSRV.Reset();

        HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, ctx->sharedTexture.GetAddressOf());
        if (FAILED(hr)) {
            logger::error("[WebGL] Failed to create shared D3D11 texture: 0x{:X}", static_cast<uint32_t>(hr));
            return false;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        hr = device->CreateShaderResourceView(ctx->sharedTexture.Get(), &srvDesc, ctx->sharedSRV.GetAddressOf());
        if (FAILED(hr)) {
            logger::error("[WebGL] Failed to create SRV for shared texture: 0x{:X}", static_cast<uint32_t>(hr));
            ctx->sharedTexture.Reset();
            return false;
        }

        return true;
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
        const EGLint configAttribs[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_STENCIL_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_NONE};

        EGLint numConfigs = 0;
        if (!eglChooseConfig(g_ANGLEDisplay, configAttribs, &ctx->eglConfig, 1, &numConfigs) || numConfigs == 0) {
            logger::error("[WebGL] eglChooseConfig failed: 0x{:X}", eglGetError());
            delete ctx;
            return nullptr;
        }

        // Create the shared D3D11 texture on Skyrim's device for compositing.
        // ANGLE renders to its own internal surface (on its own D3D11 device),
        // and we copy pixels via glReadPixels + UpdateSubresource each frame.
        if (!CreateSharedTexture(ctx, width, height, device)) {
            delete ctx;
            return nullptr;
        }

        // Create a regular pbuffer surface on ANGLE's own D3D11 device.
        // We cannot use eglCreatePbufferFromClientBuffer because ANGLE's
        // device and Skyrim's device are separate — the shared texture lives
        // on Skyrim's device.  ReadbackToSharedTexture handles the cross-device copy.
        const EGLint surfaceAttribs[] = {
            EGL_WIDTH, static_cast<EGLint>(width),
            EGL_HEIGHT, static_cast<EGLint>(height),
            EGL_NONE};

        ctx->eglSurface = eglCreatePbufferSurface(g_ANGLEDisplay, ctx->eglConfig, surfaceAttribs);
        if (ctx->eglSurface == EGL_NO_SURFACE) {
            logger::error("[WebGL] eglCreatePbufferSurface failed: 0x{:X}", eglGetError());
            delete ctx;
            return nullptr;
        }
        logger::info("[WebGL] Created pbuffer surface (ANGLE has its own D3D11 device)");

        // Create an OpenGL ES 3.0 context
        const EGLint contextAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE};

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

        logger::info("[WebGL] Created WebGL context: {}x{}, GL_RENDERER={}", width, height,
                     reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        logger::info("[WebGL] GL_VERSION={}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        logger::info("[WebGL] GL_EXTENSIONS={}", reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));

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

        // Destroy old surface
        if (ctx->eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(g_ANGLEDisplay, ctx->eglSurface);
            ctx->eglSurface = EGL_NO_SURFACE;
        }

        // Create new shared texture at new size
        if (!CreateSharedTexture(ctx, width, height, device)) {
            return false;
        }

        // Create new pbuffer surface on ANGLE's own device
        const EGLint surfaceAttribs[] = {
            EGL_WIDTH, static_cast<EGLint>(width),
            EGL_HEIGHT, static_cast<EGLint>(height),
            EGL_NONE};

        ctx->eglSurface = eglCreatePbufferSurface(g_ANGLEDisplay, ctx->eglConfig, surfaceAttribs);
        if (ctx->eglSurface == EGL_NO_SURFACE) {
            logger::error("[WebGL] ResizeWebGLContext: eglCreatePbufferSurface failed: 0x{:X}", eglGetError());
            return false;
        }

        // Rebind context
        if (!eglMakeCurrent(g_ANGLEDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext)) {
            logger::error("[WebGL] ResizeWebGLContext: eglMakeCurrent failed: 0x{:X}", eglGetError());
            return false;
        }

        ctx->canvasWidth = width;
        ctx->canvasHeight = height;

        logger::info("[WebGL] Resized WebGL context to {}x{}", width, height);
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

        if (g_ANGLEDisplay != EGL_NO_DISPLAY) {
            eglMakeCurrent(g_ANGLEDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (ctx->eglContext != EGL_NO_CONTEXT) {
                eglDestroyContext(g_ANGLEDisplay, ctx->eglContext);
            }
            if (ctx->eglSurface != EGL_NO_SURFACE) {
                eglDestroySurface(g_ANGLEDisplay, ctx->eglSurface);
            }
        }

        ctx->sharedTexture.Reset();
        ctx->sharedSRV.Reset();
        ctx->initialized = false;

        logger::info("[WebGL] Destroyed WebGL context");
        delete ctx;
    }

    void ShutdownANGLE() {
        if (g_ANGLEDisplay != EGL_NO_DISPLAY) {
            eglTerminate(g_ANGLEDisplay);
            g_ANGLEDisplay = EGL_NO_DISPLAY;
        }
        g_ANGLEInitialized = false;
        logger::info("[WebGL] ANGLE shut down");
    }

}  // namespace PrismaUI::WebGL
