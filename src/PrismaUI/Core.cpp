#include "Core.h"

#include <eh.h>  // For _set_se_translator

#include "Communication.h"
#include "GPU/GPUDriverD3D11.h"
#include "InputHandler.h"
#include "Inspector.h"
#include "Listeners.h"
#include "Utils/DllLoader.h"
#include "ViewManager.h"
#include "ViewOperationQueue.h"
#include "ViewRenderer.h"
#include "WASM/WASMRuntime.h"
#include "WebGL/ANGLEContext.h"
#include "WebGL/WebGLBridge.h"

namespace {
    // SEH exception class to convert structured exceptions to C++ exceptions
    // Copies all relevant data from EXCEPTION_POINTERS since that pointer is only
    // valid during the translator call
    class SEHException : public std::exception {
    public:
        SEHException(unsigned int code, EXCEPTION_POINTERS* ep)
            : code_(code), address_(nullptr), accessType_(0), accessAddress_(0) {
            if (ep && ep->ExceptionRecord) {
                address_ = ep->ExceptionRecord->ExceptionAddress;
                // For access violations, capture the operation type and target address
                if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
                    accessType_ = ep->ExceptionRecord->ExceptionInformation[0];
                    accessAddress_ = ep->ExceptionRecord->ExceptionInformation[1];
                }
            }
        }

        const char* what() const noexcept override { return "Windows Structured Exception"; }

        unsigned int code() const { return code_; }
        void* address() const { return address_; }

        std::string details() const {
            std::string msg;
            switch (code_) {
                case EXCEPTION_ACCESS_VIOLATION:
                    msg = "Access Violation";
                    {
                        const char* op = accessType_ == 0 ? "read" : "write";
                        char buf[128];
                        snprintf(buf, sizeof(buf), " (%s at 0x%p)", op, (void*)accessAddress_);
                        msg += buf;
                    }
                    break;
                case EXCEPTION_STACK_OVERFLOW:
                    msg = "Stack Overflow";
                    break;
                case EXCEPTION_INT_DIVIDE_BY_ZERO:
                    msg = "Integer Divide by Zero";
                    break;
                default:
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Code 0x%08X", code_);
                    msg = buf;
                    break;
            }
            return msg;
        }

    private:
        unsigned int code_;
        void* address_;
        ULONG_PTR accessType_;     // 0 = read, 1 = write, 8 = DEP violation
        ULONG_PTR accessAddress_;  // Address that was accessed
    };

    void SEHTranslator(unsigned int code, EXCEPTION_POINTERS* ep) {
        // Stack overflow cannot be safely translated to a C++ exception because
        // exception handling requires stack space for unwinding, which we don't have.
        // Let it propagate as an SEH exception - the system will terminate the
        // process.
        if (code == EXCEPTION_STACK_OVERFLOW) {
            // Don't throw - just return and let the SEH continue
            // The process will likely terminate, but that's safer than undefined
            // behavior
            return;
        }
        throw SEHException(code, ep);
    }
}  // namespace

namespace PrismaUI::Core {
    using namespace PrismaUI::Listeners;
    using namespace PrismaUI::ViewRenderer;
    using namespace PrismaUI::ViewManager;
    using namespace PrismaUI::InputHandler;

    SingleThreadExecutor ultralightThread;
    NanoIdGenerator generator;
    std::atomic<bool> coreInitialized = false;
    std::atomic<bool> rendererInitFailed = false;

    // Ultralight platform objects - ownership remains with caller per API docs
    static std::unique_ptr<MyUltralightLogger> ultralightLogger;

    RefPtr<Renderer> renderer;
    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    HWND hWnd = nullptr;

    RE::BSGraphics::ScreenSize screenSize;

    std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
    std::unique_ptr<DirectX::CommonStates> commonStates;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cursorTexture;
    std::unique_ptr<GPU::GPUDriverD3D11> gpuDriver;

    std::map<PrismaViewId, std::shared_ptr<PrismaView>> views;
    std::shared_mutex viewsMutex;

    std::map<std::pair<PrismaViewId, std::string>, JSCallbackData> PrismaUI::Core::jsCallbacks;
    std::mutex PrismaUI::Core::jsCallbacksMutex;

    inline REL::Relocation<Hooks::D3DPresentHook::D3DPresentFunc> RealD3dPresentFunc;

    PrismaView::~PrismaView() { ViewRenderer::ReleaseViewTexture(this); }

    void InitializeCoreSystem() {
        logger::info("Initializing PrismaUI Core System...");
        InitHooks();

        const auto basePath = Utils::GetBasePath();
        ultralightThread
            .submit([basePath]() {
                try {
                    Platform& plat = Platform::instance();
                    ultralightLogger = std::make_unique<MyUltralightLogger>();
                    plat.set_logger(ultralightLogger.get());
                    plat.set_font_loader(ultralight::GetPlatformFontLoader());

                    plat.set_file_system(ultralight::GetPlatformFileSystem(basePath.string().c_str()));

                    Config config;
                    config.resource_path_prefix = "resources/";
                    plat.set_config(config);

                    // Create GPU driver and register with platform before creating renderer
                    // D3D resources will be initialized later in InitGraphics()
                    gpuDriver = std::make_unique<GPU::GPUDriverD3D11>();
                    plat.set_gpu_driver(gpuDriver.get());

                    renderer = Renderer::Create();
                    if (!renderer) {
                        logger::critical("Failed to create Ultralight Renderer!");
                        rendererInitFailed = true;
                    } else {
                        logger::info(
                            "Ultralight Platform configured and Renderer created on UI "
                            "thread.");
                    }
                } catch (const std::exception& e) {
                    logger::critical(
                        "Exception during Ultralight Platform/Renderer init on UI "
                        "thread: {}",
                        e.what());
                    rendererInitFailed = true;
                } catch (...) {
                    logger::critical(
                        "Unknown exception during Ultralight Platform/Renderer init on "
                        "UI thread.");
                    rendererInitFailed = true;
                }
            })
            .get();

        auto ui = RE::UI::GetSingleton();
        ui->Register(FocusMenu::MENU_NAME, FocusMenu::Creator);

        logger::info("PrismaUI Core System Initialized.");
    }

    void InitHooks() {
        logger::debug("Installing D3D Present hook...");
        RealD3dPresentFunc = Hooks::D3DPresentHook::Install(&D3DPresent);
        logger::info("D3D Present hook installed.");
    }

    void InitGraphics() {
        auto* renderManager = RE::BSGraphics::Renderer::GetSingleton();
        if (!renderManager) {
            logger::critical("InitGraphics: RenderManager is null!");
            return;
        }
        auto runtimeData = renderManager->GetRuntimeData();
        if (!d3dDevice) d3dDevice = reinterpret_cast<ID3D11Device*>(runtimeData.forwarder);
        if (!d3dContext) d3dContext = reinterpret_cast<ID3D11DeviceContext*>(runtimeData.context);

        if (!hWnd && runtimeData.renderWindows && runtimeData.renderWindows->hWnd) {
            hWnd = reinterpret_cast<HWND>(runtimeData.renderWindows->hWnd);
            screenSize = renderManager->GetScreenSize();

            static std::atomic<bool> input_handler_initialized = false;
            bool expected_ih_init = false;

            if (input_handler_initialized.compare_exchange_strong(expected_ih_init, true)) {
                Initialize(hWnd, &ultralightThread, &views, &viewsMutex);

                // Schedule WndProc hook installation on the main thread (required for
                // SetWindowSubclass)
                SKSE::GetTaskInterface()->AddTask([]() {
                    if (InstallWndProcHook()) {
                        logger::info("WndProc hook installed successfully.");
                    } else {
                        logger::error("Failed to install WndProc hook!");
                    }
                });
            }
        } else if (!hWnd) {
            logger::warn("InitGraphics: Could not obtain HWND.");
        }

        if (d3dDevice && d3dContext) {
            if (!commonStates || !spriteBatch) {
                try {
                    commonStates = std::make_unique<DirectX::CommonStates>(d3dDevice);
                    spriteBatch = std::make_unique<DirectX::SpriteBatch>(d3dContext);
                    logger::info("DirectXTK SpriteBatch and CommonStates (re)initialized.");
                } catch (const std::exception& e) {
                    logger::critical("Failed to initialize DirectXTK: {}", e.what());
                    commonStates.reset();
                    spriteBatch.reset();
                }
            }

            if (!cursorTexture && d3dDevice) {
                auto cursorPath = Utils::GetBasePath() / "misc" / "cursor.png";
                HRESULT hr =
                    DirectX::CreateWICTextureFromFile(d3dDevice, cursorPath.wstring().c_str(), nullptr, &cursorTexture);
                if (SUCCEEDED(hr)) {
                    logger::info("Cursor texture loaded successfully.");
                } else {
                    logger::error("Failed to load cursor texture from '{}'. HRESULT: 0x{:08X}", cursorPath.string(),
                                  static_cast<unsigned int>(hr));
                    cursorTexture.Reset();
                }
            }

            // Initialize GPU driver D3D resources once device/context are available
            if (gpuDriver && !gpuDriver->IsD3DInitialized()) {
                gpuDriver->InitializeD3D(d3dDevice, d3dContext);
                if (!gpuDriver->IsD3DInitialized()) {
                    logger::error("InitGraphics: GPU driver D3D initialization failed. GPU-accelerated views will not render.");
                }
            }

            // Initialize ANGLE EGL display for WebGL support
            static bool angleInitAttempted = false;
            if (!angleInitAttempted) {
                angleInitAttempted = true;
                if (WebGL::InitializeANGLEDisplay(d3dDevice)) {
                    logger::info("InitGraphics: ANGLE EGL display initialized for WebGL support.");
                } else {
                    logger::warn("InitGraphics: ANGLE initialization failed. WebGL will not be available.");
                }
            }
        } else {
            logger::error("Cannot initialize DirectXTK: D3D device or context is null.");
            commonStates.reset();
            spriteBatch.reset();
        }
    }

    void D3DPresent(uint32_t a_p1) {
        RealD3dPresentFunc(a_p1);

        if (!coreInitialized || rendererInitFailed) return;

        if (!d3dDevice || !d3dContext || !spriteBatch || !commonStates || !hWnd || screenSize.width == 0) {
            InitGraphics();
            if (!d3dDevice || !d3dContext || !spriteBatch || !commonStates || !hWnd || screenSize.width == 0) return;
        }

        std::vector<PrismaViewId> viewsWithPendingRelease;
        {
            std::shared_lock lock(viewsMutex);
            for (const auto& pair : views) {
                if (pair.second && pair.second->pendingResourceRelease.load()) {
                    viewsWithPendingRelease.push_back(pair.first);
                }
            }
        }

        for (const auto& viewId : viewsWithPendingRelease) {
            std::shared_ptr<PrismaView> viewData = nullptr;
            {
                std::shared_lock lock(viewsMutex);
                auto it = views.find(viewId);
                if (it != views.end()) {
                    viewData = it->second;
                }
            }

            if (viewData) {
                logger::debug(
                    "D3DPresent: Releasing D3D resources for View [{}] from render "
                    "thread",
                    viewId);
                ViewRenderer::ReleaseViewTexture(viewData.get());
                Inspector::ReleaseInspectorTexture(viewData.get());
                viewData->pendingResourceRelease = false;
            }
        }

        // Process pending operations for all views
        ViewOperationQueue::ProcessAllViewOperations();

        auto ultralightFuture = ultralightThread.submit([dev = d3dDevice, ctx = d3dContext, hwnd = hWnd]() {
            // Enable SEH to C++ exception translation for this thread (only needs to be
            // set once per thread)
            static bool sehTranslatorSet = false;
            if (!sehTranslatorSet) {
                _set_se_translator(SEHTranslator);
                sehTranslatorSet = true;
            }

            try {
                if (!dev || !ctx || !hwnd) {
                    logger::warn("UI Thread: D3D device/context/hwnd is null, skipping frame.");
                    return;
                }

                // Capture renderer locally to avoid race with shutdown
                auto localRenderer = renderer;
                if (!localRenderer) {
                    logger::warn("UI Thread: Renderer is null, skipping frame.");
                    return;
                }

                // Check for views that need recovery after an exception
                std::vector<std::shared_ptr<PrismaView>> viewsToRecover;
                {
                    std::shared_lock lock(viewsMutex);
                    for (auto& pair : views) {
                        if (pair.second && pair.second->needsRecovery.load() && pair.second->ultralightView) {
                            viewsToRecover.push_back(pair.second);
                        }
                    }
                }

                for (auto& viewData : viewsToRecover) {
                    int attempts = viewData->recoveryAttempts.fetch_add(1);
                    if (attempts >= 3) {
                        logger::error(
                            "UI Thread: View [{}] recovery failed after {} attempts, giving "
                            "up",
                            viewData->id, attempts);
                        viewData->needsRecovery = false;
                        continue;
                    }

                    // Use original URL for recovery (the entry point that sets up
                    // everything)
                    if (!viewData->originalUrl.empty()) {
                        logger::info(
                            "UI Thread: Recovering View [{}] (attempt {}) by reloading "
                            "original URL: {}",
                            viewData->id, attempts + 1, viewData->originalUrl);
                        try {
                            viewData->ultralightView->LoadURL(String(viewData->originalUrl.c_str()));
                            viewData->needsRecovery = false;
                            viewData->isLoadingFinished = false;
                            // recoveryAttempts will be reset by OnFinishLoading on successful
                            // load
                        } catch (...) {
                            logger::error("UI Thread: Failed to initiate recovery for View [{}]", viewData->id);
                        }
                    } else {
                        logger::warn("UI Thread: View [{}] needs recovery but has no originalUrl", viewData->id);
                        viewData->needsRecovery = false;  // Clear flag to avoid infinite loop
                    }
                }

                std::vector<std::shared_ptr<PrismaView>> viewsToInitialize;
                {
                    std::shared_lock lock(viewsMutex);
                    for (auto& pair : views) {
                        if (pair.second && !pair.second->ultralightView && !pair.second->htmlPathToLoad.empty()) {
                            viewsToInitialize.push_back(pair.second);
                        }
                    }
                }

                for (auto& viewData : viewsToInitialize) {
                    if (!viewData || viewData->ultralightView) continue;

                    logger::info("UI Thread: Creating {} View [{}] for path: {}", viewData->isAccelerated ? "GPU" : "CPU", viewData->id, viewData->htmlPathToLoad);

                    if (screenSize.width == 0 || screenSize.height == 0) {
                        logger::error("UI Thread: Cannot create View [{}], screen size is zero.", viewData->id);
                        continue;
                    }

                    // Re-check renderer before creating view
                    if (!localRenderer) {
                        logger::warn("UI Thread: Renderer became null during view creation.");
                        break;
                    }

                    ViewConfig view_config;
                    view_config.is_accelerated = viewData->isAccelerated;
                    view_config.is_transparent = true;
                    view_config.initial_focus = false;
                    view_config.enable_images = true;
                    view_config.enable_javascript = true;
                    view_config.enable_compositor = false;

                    viewData->ultralightView =
                        localRenderer->CreateView(screenSize.width, screenSize.height, view_config, nullptr);

                    if (viewData->ultralightView) {
                        viewData->loadListener = std::make_unique<Listeners::MyLoadListener>(viewData->id);
                        viewData->viewListener = std::make_unique<Listeners::MyViewListener>(viewData->id);
                        viewData->ultralightView->set_load_listener(viewData->loadListener.get());
                        viewData->ultralightView->set_view_listener(viewData->viewListener.get());
                        viewData->ultralightView->LoadURL(String(viewData->htmlPathToLoad.c_str()));
                        viewData->ultralightView->Unfocus();
                        viewData->htmlPathToLoad.clear();
                        logger::info("UI Thread: View [{}] successfully created and loading URL.", viewData->id);
                    } else {
                        logger::error("UI Thread: Failed to create Ultralight View for ID [{}].", viewData->id);
                        viewData->htmlPathToLoad = "[CREATION FAILED]";
                    }
                }

                ProcessEvents();

                // Reset per-frame WebGL state so the first GL call re-syncs
                // ANGLE's D3D11 state after Ultralight may have changed it.
                WebGL::ResetFrameState();

                if (localRenderer) {
                    localRenderer->Update();

                    // Restore D3D11 render targets after WebGL/ANGLE may have
                    // changed them during Update().  Must happen before Render()
                    // which needs Ultralight's GPU driver state intact.
                    WebGL::EndFrameGLState();

                    localRenderer->RefreshDisplay(0);
                    localRenderer->Render();
                }

                RenderViews();
            } catch (const SEHException& seh) {
                logger::critical("UI Thread: SEH Exception in render loop: {} at address 0x{:p}", seh.details(),
                                 seh.address());
                // Mark all views for recovery - the renderer state is likely corrupted
                {
                    std::shared_lock lock(viewsMutex);
                    for (auto& pair : views) {
                        if (pair.second) {
                            pair.second->needsRecovery = true;
                            logger::warn("View [{}] marked for recovery after SEH exception", pair.first);
                        }
                    }
                }
            } catch (const std::exception& e) {
                logger::critical("UI Thread: Exception in render loop: {}", e.what());
                // Mark all views for recovery
                {
                    std::shared_lock lock(viewsMutex);
                    for (auto& pair : views) {
                        if (pair.second) {
                            pair.second->needsRecovery = true;
                        }
                    }
                }
            } catch (...) {
                // Unknown exceptions (likely from Ultralight/WebCore internals)
                logger::critical(
                    "UI Thread: Unknown exception in render loop (likely Ultralight "
                    "internal error)");
                // Mark all views for recovery
                {
                    std::shared_lock lock(viewsMutex);
                    for (auto& pair : views) {
                        if (pair.second) {
                            pair.second->needsRecovery = true;
                        }
                    }
                }
            }
        });

        // Wait for UI thread but handle any exceptions that might have escaped
        try {
            ultralightFuture.get();
        } catch (const std::exception& e) {
            logger::error("D3DPresent: Exception from UI thread: {}", e.what());
        } catch (...) {
            logger::error("D3DPresent: Unknown exception from UI thread");
        }

        // Execute GPU driver commands on render thread (D3D state save/restore)
        if (gpuDriver && gpuDriver->IsD3DInitialized() && gpuDriver->HasPendingCommands()) {
            // Save all D3D state that the GPU driver modifies (using ComPtr for exception safety)
            Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backupRTV;
            Microsoft::WRL::ComPtr<ID3D11DepthStencilView> backupDSV;
            D3D11_VIEWPORT backupViewport = {};
            UINT numViewports = 1;
            Microsoft::WRL::ComPtr<ID3D11BlendState> backupBlend;
            FLOAT backupBlendFactor[4] = {};
            UINT backupSampleMask = 0;
            Microsoft::WRL::ComPtr<ID3D11RasterizerState> backupRasterizer;
            Microsoft::WRL::ComPtr<ID3D11DepthStencilState> backupDepthStencil;
            UINT backupStencilRef = 0;

            // Shaders & input layout
            Microsoft::WRL::ComPtr<ID3D11VertexShader> backupVS;
            Microsoft::WRL::ComPtr<ID3D11PixelShader> backupPS;
            Microsoft::WRL::ComPtr<ID3D11InputLayout> backupInputLayout;

            // IA state
            Microsoft::WRL::ComPtr<ID3D11Buffer> backupVB;
            UINT backupVBStride = 0;
            UINT backupVBOffset = 0;
            Microsoft::WRL::ComPtr<ID3D11Buffer> backupIB;
            DXGI_FORMAT backupIBFormat = DXGI_FORMAT_UNKNOWN;
            UINT backupIBOffset = 0;
            D3D11_PRIMITIVE_TOPOLOGY backupTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

            // Constant buffers (slot 0)
            Microsoft::WRL::ComPtr<ID3D11Buffer> backupVSCB;
            Microsoft::WRL::ComPtr<ID3D11Buffer> backupPSCB;

            // PS shader resources (slots 0-2) and samplers (slot 0)
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> backupSRV[3];
            Microsoft::WRL::ComPtr<ID3D11SamplerState> backupSampler;

            // Scissor rects
            D3D11_RECT backupScissorRect = {};
            UINT numScissorRects = 1;

            // --- Save state ---
            d3dContext->OMGetRenderTargets(1, backupRTV.GetAddressOf(), backupDSV.GetAddressOf());
            d3dContext->RSGetViewports(&numViewports, &backupViewport);
            d3dContext->OMGetBlendState(backupBlend.GetAddressOf(), backupBlendFactor, &backupSampleMask);
            d3dContext->RSGetState(backupRasterizer.GetAddressOf());
            d3dContext->OMGetDepthStencilState(backupDepthStencil.GetAddressOf(), &backupStencilRef);

            d3dContext->VSGetShader(backupVS.GetAddressOf(), nullptr, nullptr);
            d3dContext->PSGetShader(backupPS.GetAddressOf(), nullptr, nullptr);
            d3dContext->IAGetInputLayout(backupInputLayout.GetAddressOf());
            d3dContext->IAGetVertexBuffers(0, 1, backupVB.GetAddressOf(), &backupVBStride, &backupVBOffset);
            d3dContext->IAGetIndexBuffer(backupIB.GetAddressOf(), &backupIBFormat, &backupIBOffset);
            d3dContext->IAGetPrimitiveTopology(&backupTopology);
            d3dContext->VSGetConstantBuffers(0, 1, backupVSCB.GetAddressOf());
            d3dContext->PSGetConstantBuffers(0, 1, backupPSCB.GetAddressOf());

            ID3D11ShaderResourceView* rawSRVs[3] = {};
            d3dContext->PSGetShaderResources(0, 3, rawSRVs);
            for (int i = 0; i < 3; ++i) { backupSRV[i].Attach(rawSRVs[i]); }

            d3dContext->PSGetSamplers(0, 1, backupSampler.GetAddressOf());
            d3dContext->RSGetScissorRects(&numScissorRects, &backupScissorRect);

            gpuDriver->DrawCommandList();

            // --- Restore state ---
            d3dContext->OMSetRenderTargets(1, backupRTV.GetAddressOf(), backupDSV.Get());
            d3dContext->RSSetViewports(numViewports, &backupViewport);
            d3dContext->OMSetBlendState(backupBlend.Get(), backupBlendFactor, backupSampleMask);
            d3dContext->RSSetState(backupRasterizer.Get());
            d3dContext->OMSetDepthStencilState(backupDepthStencil.Get(), backupStencilRef);

            d3dContext->VSSetShader(backupVS.Get(), nullptr, 0);
            d3dContext->PSSetShader(backupPS.Get(), nullptr, 0);
            d3dContext->IASetInputLayout(backupInputLayout.Get());
            d3dContext->IASetVertexBuffers(0, 1, backupVB.GetAddressOf(), &backupVBStride, &backupVBOffset);
            d3dContext->IASetIndexBuffer(backupIB.Get(), backupIBFormat, backupIBOffset);
            d3dContext->IASetPrimitiveTopology(backupTopology);
            d3dContext->VSSetConstantBuffers(0, 1, backupVSCB.GetAddressOf());
            d3dContext->PSSetConstantBuffers(0, 1, backupPSCB.GetAddressOf());

            ID3D11ShaderResourceView* restoreSRVs[3] = {backupSRV[0].Get(), backupSRV[1].Get(), backupSRV[2].Get()};
            d3dContext->PSSetShaderResources(0, 3, restoreSRVs);

            d3dContext->PSSetSamplers(0, 1, backupSampler.GetAddressOf());
            if (numScissorRects > 0) d3dContext->RSSetScissorRects(numScissorRects, &backupScissorRect);
        }

        std::vector<std::shared_ptr<PrismaView>> viewsToCheck;
        {
            std::shared_lock lock(viewsMutex);
            viewsToCheck.reserve(views.size());
            for (const auto& pair : views) {
                if (pair.second && pair.second->ultralightView) {
                    viewsToCheck.push_back(pair.second);
                }
            }
        }

        for (const auto& viewData : viewsToCheck) {
            if (!viewData->isAccelerated) {
                UpdateSingleTextureFromBuffer(viewData);
            }
        }

        DrawViews();
        DrawCursor();
    }

    void Shutdown() {
        logger::info("Shutting down PrismaUI Core System...");

        std::vector<PrismaViewId> viewIdsToDestroy;
        {
            std::shared_lock lock(viewsMutex);
            for (const auto& pair : views) {
                viewIdsToDestroy.push_back(pair.first);
            }
        }

        for (const auto& id : viewIdsToDestroy) {
            try {
                ViewManager::Destroy(id);
            } catch (const std::exception& e) {
                logger::error("Error destroying view [{}] during shutdown: {}", id, e.what());
            }
        }

        cursorTexture.Reset();
        spriteBatch.reset();
        commonStates.reset();
        logger::debug("DirectXTK resources released.");

        InputHandler::Shutdown();

        d3dDevice = nullptr;
        d3dContext = nullptr;
        hWnd = nullptr;

        {
            std::unique_lock lock(viewsMutex);
            views.clear();
        }

        // Force-shutdown WAMR runtime (safety net — individual instances cleaned up in Destroy)
        WASM::ForceShutdownRuntime();

        if (renderer) {
            // Move renderer to the lambda so it's the sole owner,
            // ensuring release happens on the UI thread
            ultralightThread
                .submit([renderer_moved = std::move(renderer)]() mutable {
                    logger::info("Releasing global renderer on UI thread.");
                    renderer_moved = nullptr;
                })
                .get();
        }

        // GPU driver must be destroyed after the renderer, since the Ultralight
        // Platform holds a raw pointer (set via set_gpu_driver) and the renderer's
        // destructor may call back into the driver to release GPU resources.
        gpuDriver.reset();
        logger::debug("GPU driver resources released.");

        // Release Ultralight platform objects after renderer is destroyed
        ultralightLogger.reset();

        coreInitialized = false;
        logger::info("PrismaUI Core System shut down complete.");
    }
}  // namespace PrismaUI::Core
