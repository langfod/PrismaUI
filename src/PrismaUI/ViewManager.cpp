#include "ViewManager.h"

#include "Core.h"
#include "Audio/AudioContext.h"
#include "InputHandler.h"
#include "Inspector.h"
#include "Listeners.h"
#include "ViewOperationQueue.h"
#include "WASM/WASMRuntime.h"
#include "WebGL/ANGLEContext.h"

namespace PrismaUI::ViewManager {
    using namespace Core;

    // Cached focus state — updated by Focus/Unfocus on the ultralight thread,
    // read lock-free by HasAnyActiveFocus (Scaleform AdvanceMovie_Hook).
    static std::atomic<bool> g_anyViewHasFocus{false};

    Core::PrismaViewId Create(const std::string& htmlPath, std::function<void(Core::PrismaViewId)> onDomReadyCallback) {
        return Create(htmlPath, Acceleration::Default, std::move(onDomReadyCallback));
    }

    Core::PrismaViewId Create(const std::string& htmlPath, Acceleration accel,
                              std::function<void(Core::PrismaViewId)> onDomReadyCallback) {
        bool isAccelerated = (accel == Acceleration::Accelerated);
        bool expected_init = false;
        if (coreInitialized.compare_exchange_strong(expected_init, true)) {
            Core::InitializeCoreSystem();
            if (!renderer) {
                coreInitialized = false;
                logger::critical("Core initialization failed: Renderer not created.");
                throw std::runtime_error("PrismaUI Core Renderer initialization failed.");
            }
        } else if (!renderer) {
            logger::critical("Cannot create HTML view: Core Renderer is null despite initialization flag.");
            throw std::runtime_error("PrismaUI Core Renderer is unexpectedly null.");
        }

        Core::PrismaViewId newViewId = generator.generate();

        // Check if htmlPath is a website URL (starts with http:// or https://)
        std::string fileUrl;
        if (htmlPath.substr(0, 7) == "http://" || htmlPath.substr(0, 8) == "https://") {
            fileUrl = htmlPath;
        } else {
            fileUrl = "file:///views/" + htmlPath;
        }

        auto viewData = std::make_shared<Core::PrismaView>();
        viewData->id = newViewId;
        viewData->ultralightView = nullptr;
        viewData->htmlPathToLoad = fileUrl;
        viewData->originalUrl = fileUrl;  // Store for recovery after exceptions
        viewData->isAccelerated = isAccelerated;
        viewData->isHidden = false;
        viewData->domReadyCallback = std::move(onDomReadyCallback);

        {
            std::unique_lock lock(viewsMutex);
            int maxOrder = -1;
            if (views.empty()) {
                viewData->order = 0;
            } else {
                for (const auto& pair : views) {
                    if (pair.second->order > maxOrder) {
                        maxOrder = pair.second->order;
                    }
                }
                viewData->order = maxOrder + 1;
            }
            views[newViewId] = viewData;
        }

        logger::info(
            "View [{}] creation requested for path: {} with order <{}>, accelerated={}. Actual view will be created by "
            "UI thread.",
            newViewId, fileUrl, viewData->order, isAccelerated);

        return newViewId;
    }

    // Helper function to perform unfocus operations (used by Hide, Unfocus, and Focus)
    // closeFocusMenu: whether to close FocusMenu (false when switching focus between views)
    static void PerformUnfocusOperations(const Core::PrismaViewId& viewId, std::shared_ptr<PrismaView> viewData,
                                         bool closeFocusMenu = true) {
        if (!viewData || !viewData->ultralightView) {
            return;
        }

        if (viewData->isPaused.load()) {
            auto ui = RE::UI::GetSingleton();
            if (ui && ui->numPausesGame > 0) {
                ui->numPausesGame--;
            }
            viewData->isPaused.store(false);
        }

        PrismaUI::InputHandler::DisableInputCapture(viewId);
        viewData->ultralightView->Unfocus();

        if (closeFocusMenu) {
            // Full unfocus — no other view is taking focus, safe to clear
            g_anyViewHasFocus.store(false, std::memory_order_release);
            FocusMenu::Close();
        }
        // When closeFocusMenu==false we are switching focus between views;
        // the new view already set g_anyViewHasFocus=true, don't clobber it.

        auto controlMap = RE::ControlMap::GetSingleton();
        controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom, true, false);
        controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kLooking, true, false);
        controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kJumping, true, false);
        controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMovement, true, false);
        controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kActivate, true, false);
        controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kPOVSwitch, true, false);
        controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kVATS, true, false);
    }

    void Show(const Core::PrismaViewId& viewId) {
        if (!ViewManager::IsValid(viewId)) {
            logger::warn("Show: View ID [{}] not found.", viewId);
            return;
        }

        ViewOperationQueue::EnqueueOperation(viewId, [viewId]() {
            std::shared_ptr<PrismaView> viewData = nullptr;
            {
                std::shared_lock lock(viewsMutex);
                auto it = views.find(viewId);
                if (it != views.end()) {
                    viewData = it->second;
                }
            }

            if (viewData) {
                if (!viewData->isHidden.load()) {
                    logger::debug("Show: View [{}] is already visible.", viewId);
                    return;
                }
                viewData->isHidden = false;
                logger::debug("View [{}] marked as Visible.", viewId);
            }
        });
    }

    void Hide(const Core::PrismaViewId& viewId) {
        if (!ViewManager::IsValid(viewId)) {
            logger::warn("Hide: View ID [{}] not found.", viewId);
            return;
        }

        ViewOperationQueue::EnqueueOperation(viewId, [viewId]() {
            std::shared_ptr<PrismaView> viewData = nullptr;
            {
                std::shared_lock lock(viewsMutex);
                auto it = views.find(viewId);
                if (it != views.end()) {
                    viewData = it->second;
                }
            }

            if (viewData) {
                if (viewData->isHidden.load()) {
                    logger::debug("Hide: View [{}] is already hidden.", viewId);
                    return;
                }

                // If view has focus, unfocus it first
                if (viewData->ultralightView && viewData->ultralightView->HasFocus()) {
                    PerformUnfocusOperations(viewId, viewData);
                    logger::debug("Hide: View [{}] was focused, unfocused it.", viewId);
                }

                viewData->isHidden = true;
                logger::debug("View [{}] marked as Hidden.", viewId);
            }
        });
    }

    bool IsHidden(const Core::PrismaViewId& viewId) {
        std::shared_lock lock(viewsMutex);
        auto it = views.find(viewId);
        if (it != views.end()) {
            return it->second->isHidden.load();
        }
        logger::warn("IsHidden: View ID [{}] not found.", viewId);
        return true;
    }

    bool IsValid(const Core::PrismaViewId& viewId) {
        std::shared_lock lock(viewsMutex);
        return views.find(viewId) != views.end();
    }

    bool Focus(const Core::PrismaViewId& viewId, bool pauseGame, bool disableFocusMenu) {
        if (!ViewManager::IsValid(viewId)) {
            logger::warn("Focus: View ID [{}] not found.", viewId);
            return false;
        }

        ViewOperationQueue::EnqueueOperation(viewId, [viewId, pauseGame, disableFocusMenu]() {
            std::shared_ptr<PrismaView> viewData = nullptr;
            {
                std::shared_lock lock(viewsMutex);
                auto it = views.find(viewId);
                if (it != views.end()) {
                    viewData = it->second;
                }
            }

            if (!viewData || !viewData->ultralightView) {
                logger::warn("Focus: View [{}] or its Ultralight View is not ready.", viewId);
                return;
            }

            if (viewData->isHidden.load()) {
                logger::warn("Focus: View [{}] is hidden, cannot focus.", viewId);
                return;
            }

            if (viewData->ultralightView->HasFocus()) {
                logger::debug("Focus: View [{}] already has focus.", viewId);
                return;
            }

            // Unfocus other views
            std::vector<Core::PrismaViewId> viewsToUnfocus;
            {
                std::shared_lock lock(viewsMutex);
                for (const auto& pair : views) {
                    if (pair.first != viewId && pair.second->ultralightView &&
                        pair.second->ultralightView->HasFocus()) {
                        viewsToUnfocus.push_back(pair.first);
                    }
                }
            }

            for (const auto& idToUnfocus : viewsToUnfocus) {
                ViewOperationQueue::EnqueueOperation(idToUnfocus, [idToUnfocus]() {
                    std::shared_ptr<PrismaView> vd = nullptr;
                    {
                        std::shared_lock lock(viewsMutex);
                        auto it = views.find(idToUnfocus);
                        if (it != views.end()) {
                            vd = it->second;
                        }
                    }

                    if (vd && vd->ultralightView) {
                        // Don't close FocusMenu when switching focus between views
                        PerformUnfocusOperations(idToUnfocus, vd, false);
                        logger::debug("Unfocus: View [{}] unfocused (focus switching).", idToUnfocus);
                    }
                });
            }

            // Focus this view
            viewData->ultralightView->Focus();
            g_anyViewHasFocus.store(true, std::memory_order_release);
            PrismaUI::InputHandler::EnableInputCapture(viewId);

            if (!disableFocusMenu) {
                FocusMenu::Open();
            }

            auto controlMap = RE::ControlMap::GetSingleton();
            controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kWheelZoom, false, false);
            controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kLooking, false, false);
            controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kJumping, false, false);
            controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMovement, false, false);
            controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kActivate, false, false);
            controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kPOVSwitch, false, false);
            controlMap->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kVATS, false, false);

            if (pauseGame) {
                auto ui = RE::UI::GetSingleton();
                if (ui) {
                    ui->numPausesGame++;
                    viewData->isPaused.store(true);
                    logger::debug("Game paused for View [{}]", viewId);
                }
            }

            logger::debug("Focus: View [{}] focused successfully.", viewId);
        });

        return true;
    }

    void Unfocus(const Core::PrismaViewId& viewId) {
        if (!ViewManager::IsValid(viewId)) {
            logger::warn("Unfocus: View ID [{}] not found.", viewId);
            return;
        }

        ViewOperationQueue::EnqueueOperation(viewId, [viewId]() {
            std::shared_ptr<PrismaView> viewData = nullptr;
            {
                std::shared_lock lock(viewsMutex);
                auto it = views.find(viewId);
                if (it != views.end()) {
                    viewData = it->second;
                }
            }

            if (!viewData) {
                logger::warn("Unfocus: View [{}] not found during operation execution.", viewId);
                g_anyViewHasFocus.store(false, std::memory_order_release);
                PrismaUI::InputHandler::DisableInputCapture(0);
                FocusMenu::Close();
                return;
            }

            if (!viewData->ultralightView) {
                logger::warn("Unfocus: View [{}] Ultralight View is not ready.", viewId);
                if (viewData->isPaused.load()) {
                    auto ui = RE::UI::GetSingleton();
                    if (ui && ui->numPausesGame > 0) {
                        ui->numPausesGame--;
                    }
                    viewData->isPaused.store(false);
                }
                g_anyViewHasFocus.store(false, std::memory_order_release);
                PrismaUI::InputHandler::DisableInputCapture(viewId);
                FocusMenu::Close();
                return;
            }

            if (!viewData->ultralightView->HasFocus()) {
                logger::debug("Unfocus: View [{}] does not have focus.", viewId);
                return;
            }

            PerformUnfocusOperations(viewId, viewData);
            logger::debug("Unfocus: View [{}] unfocused successfully.", viewId);
        });
    }

    bool HasFocus(const Core::PrismaViewId& viewId) {
        std::shared_ptr<PrismaView> viewData = nullptr;
        {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(viewId);
            if (it != views.end()) {
                viewData = it->second;
            }
        }

        if (viewData) {
            auto checkFocus = [view_ptr = viewData->ultralightView]() -> bool {
                return view_ptr ? view_ptr->HasFocus() : false;
            };
            if (ultralightThread.IsWorkerThread()) {
                return checkFocus();
            }
            auto future = ultralightThread.submit(std::move(checkFocus));
            try {
                return WaitWithMessagePump(future);
            } catch (const std::exception& e) {
                logger::error("Exception getting focus state for View [{}]: {}", viewId, e.what());
                return false;
            }
        } else {
            logger::warn("HasFocus: View ID [{}] not found.", viewId);
            return false;
        }
    }

    bool ViewHasInputFocus(const Core::PrismaViewId& viewId) {
        std::shared_ptr<PrismaView> viewData = nullptr;
        {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(viewId);
            if (it != views.end()) {
                viewData = it->second;
            }
        }

        if (viewData && viewData->ultralightView) {
            auto checkInputFocus = [view_ptr = viewData->ultralightView]() -> bool {
                if (view_ptr) {
                    return view_ptr->HasInputFocus();
                }
                return false;
            };
            if (ultralightThread.IsWorkerThread()) {
                return checkInputFocus();
            }
            auto future = ultralightThread.submit(std::move(checkInputFocus));
            try {
                return WaitWithMessagePump(future);
            } catch (const std::exception& e) {
                logger::error("View [{}]: Exception in ViewHasInputFocus: {}", viewId, e.what());
                return false;
            }
        }
        return false;
    }

    void SetScrollingPixelSize(const Core::PrismaViewId& viewId, int pixelSize) {
        std::unique_lock lock(viewsMutex);
        auto it = views.find(viewId);
        if (it != views.end()) {
            if (pixelSize <= 0) {
                logger::warn("SetScrollingPixelSize: Invalid pixel size {} for view [{}]. Must be > 0. Using default.",
                             pixelSize, viewId);
                it->second->scrollingPixelSize = 16;
            } else {
                it->second->scrollingPixelSize = pixelSize;
                logger::debug("SetScrollingPixelSize: Set {} pixels per scroll line for view [{}]", pixelSize, viewId);
            }
        } else {
            logger::warn("SetScrollingPixelSize: View ID [{}] not found.", viewId);
        }
    }

    int GetScrollingPixelSize(const Core::PrismaViewId& viewId) {
        std::shared_lock lock(viewsMutex);
        auto it = views.find(viewId);
        if (it != views.end()) {
            return it->second->scrollingPixelSize;
        }
        logger::warn("GetScrollingPixelSize: View ID [{}] not found, returning default.", viewId);
        return 28;
    }

    void Destroy(const Core::PrismaViewId& viewId) {
        logger::info("Destroy: Beginning destruction of View [{}]", viewId);

        if (!ViewManager::IsValid(viewId)) {
            logger::warn("Destroy: View ID [{}] not found.", viewId);
            return;
        }

        // Clear pending operations for this view to prevent execution after destruction
        ViewOperationQueue::ClearOperations(viewId);
        logger::debug("Destroy: Cleared pending operations for View [{}]", viewId);

        if (HasFocus(viewId)) {
            logger::debug("Destroy: View [{}] has focus, unfocusing first.", viewId);
            Unfocus(viewId);
        }

        std::shared_ptr<PrismaView> viewDataToDestroy = nullptr;
        {
            std::unique_lock lock(viewsMutex);
            auto it = views.find(viewId);
            if (it != views.end()) {
                viewDataToDestroy = std::move(it->second);
                views.erase(it);
                logger::debug("Destroy: Removed View [{}] from views map", viewId);
            } else {
                logger::warn("Destroy: View ID [{}] not found after checking validity.", viewId);
                return;
            }
        }

        viewDataToDestroy->isHidden = true;
        logger::debug("Destroy: Marked View [{}] as hidden", viewId);

        {
            std::lock_guard<std::mutex> lock(jsCallbacksMutex);
            logger::debug("Destroy: Removing JavaScript callbacks for View [{}]", viewId);

            auto it = jsCallbacks.begin();
            size_t removedCallbacks = 0;

            while (it != jsCallbacks.end()) {
                if (it->first.first == viewId) {
                    it = jsCallbacks.erase(it);
                    removedCallbacks++;
                } else {
                    ++it;
                }
            }

            if (removedCallbacks > 0) {
                logger::debug("Destroy: Removed {} JavaScript callback(s) for View [{}]", removedCallbacks, viewId);
            }
        }

        logger::debug("Destroy: Cleaning up Ultralight resources (on UI thread) for View [{}]", viewId);
        auto cleanupTask = [viewId, viewData = viewDataToDestroy]() -> bool {
            try {
                logger::debug("Destroy: Beginning Ultralight resources cleanup for View [{}]", viewId);

                // Clean up WebGL context first (must happen on ultralight thread where it was created)
                auto* wglCtx = viewData->webglContext.load(std::memory_order_acquire);
                if (wglCtx) {
                    logger::debug("Destroy: Releasing WebGL context for View [{}]", viewId);
                    WebGL::DestroyWebGLContext(wglCtx);
                    viewData->webglContext.store(nullptr, std::memory_order_release);
                }

                // Clean up WASM instances
                if (!viewData->wasmInstances.empty()) {
                    logger::debug("Destroy: Releasing {} WASM instance(s) for View [{}]",
                                  viewData->wasmInstances.size(), viewId);
                    for (auto& inst : viewData->wasmInstances) {
                        WASM::DestroyInstance(inst);
                    }
                    viewData->wasmInstances.clear();
                }

                // Clean up audio context
                if (viewData->audioContext) {
                    logger::debug("Destroy: Releasing AudioContext for View [{}]", viewId);
                    Audio::DestroyAudioContext(viewData->audioContext);
                    viewData->audioContext = nullptr;
                }

                // Clean up inspector resources first
                if (viewData->inspectorView) {
                    logger::debug("Destroy: Releasing inspector view for View [{}]", viewId);
                    viewData->inspectorView = nullptr;
                }
                Inspector::DestroyInspectorResources(viewData.get());

                if (viewData->ultralightView) {
                    logger::debug("Destroy: Detaching listeners for View [{}]", viewId);
                    viewData->ultralightView->set_load_listener(nullptr);
                    viewData->ultralightView->set_view_listener(nullptr);

                    viewData->loadListener.reset();
                    viewData->viewListener.reset();

                    viewData->ultralightView = nullptr;
                    logger::debug("Destroy: Ultralight View object released for View [{}]", viewId);
                }

                {
                    std::lock_guard lock(viewData->bufferMutex);
                    viewData->pixelBuffer.clear();
                    viewData->pixelBuffer.shrink_to_fit();
                    viewData->bufferWidth = 0;
                    viewData->bufferHeight = 0;
                    viewData->bufferStride = 0;
                    logger::debug("Destroy: Pixel buffer cleared for View [{}]", viewId);
                }

                viewData->isLoadingFinished = false;
                viewData->newFrameReady = false;

                logger::debug("Destroy: Ultralight resources for View [{}] cleaned up successfully", viewId);

                return true;
            } catch (const std::exception& e) {
                logger::error("Destroy: Exception during Ultralight resource cleanup for View [{}]: {}", viewId,
                              e.what());
                return false;
            } catch (...) {
                logger::error("Destroy: Unknown exception during Ultralight resource cleanup for View [{}]", viewId);
                return false;
            }
        };

        bool ultralight_success = false;
        if (ultralightThread.IsWorkerThread()) {
            ultralight_success = cleanupTask();
        } else {
            auto ultralightCleanupFuture = ultralightThread.submit(std::move(cleanupTask));
            try {
                ultralight_success = WaitWithMessagePump(ultralightCleanupFuture);
            } catch (const std::exception& e) {
                logger::error("Destroy: Exception waiting for Ultralight cleanup for View [{}]: {}", viewId, e.what());
            }
        }
        if (ultralight_success) {
            logger::debug("Destroy: Ultralight resources cleanup completed successfully for View [{}]", viewId);
        } else {
            logger::warn("Destroy: Ultralight resources cleanup reported failure for View [{}]", viewId);
        }

        bool hasD3DResources = (viewDataToDestroy->texture != nullptr || viewDataToDestroy->textureView != nullptr);

        if (hasD3DResources) {
            logger::debug("Destroy: D3D resources present for View [{}], forcing manual cleanup", viewId);

            if (viewDataToDestroy->textureView) {
                logger::debug("Destroy: Releasing textureView for View [{}]", viewId);
                viewDataToDestroy->textureView->Release();
                viewDataToDestroy->textureView = nullptr;
            }

            if (viewDataToDestroy->texture) {
                logger::debug("Destroy: Releasing texture for View [{}]", viewId);
                viewDataToDestroy->texture->Release();
                viewDataToDestroy->texture = nullptr;
            }

            viewDataToDestroy->textureWidth = 0;
            viewDataToDestroy->textureHeight = 0;

            logger::debug("Destroy: D3D resources released for View [{}]", viewId);
        } else {
            logger::debug("Destroy: No D3D resources to release for View [{}]", viewId);
        }

        viewDataToDestroy->pendingResourceRelease = false;

        logger::info("Destroy: View [{}] successfully destroyed", viewId);
    }

    void SetOrder(const Core::PrismaViewId& viewId, int order) {
        std::unique_lock lock(viewsMutex);
        auto it = views.find(viewId);
        if (it != views.end()) {
            it->second->order = order;
            logger::debug("SetOrder: Set order {} for view [{}]", order, viewId);
        } else {
            logger::warn("SetOrder: View ID [{}] not found.", viewId);
        }
    }

    int GetOrder(const Core::PrismaViewId& viewId) {
        std::shared_lock lock(viewsMutex);
        auto it = views.find(viewId);
        if (it != views.end()) {
            return it->second->order;
        }
        logger::warn("GetOrder: View ID [{}] not found, returning -1.", viewId);
        return -1;
    }

    // ========== Inspector API Wrappers ==========

    void CreateInspectorView(const Core::PrismaViewId& viewId) { Inspector::CreateInspectorView(viewId); }

    void SetInspectorVisibility(const Core::PrismaViewId& viewId, bool visible) {
        Inspector::SetInspectorVisibility(viewId, visible);
    }

    bool IsInspectorVisible(const Core::PrismaViewId& viewId) { return Inspector::IsInspectorVisible(viewId); }

    void SetInspectorBounds(const Core::PrismaViewId& viewId, float topLeftX, float topLeftY, uint32_t width,
                            uint32_t height) {
        Inspector::SetInspectorBounds(viewId, topLeftX, topLeftY, width, height);
    }

    bool HasAnyActiveFocus() {
        // Fast path: return cached state (updated by Focus/Unfocus on the
        // ultralight thread).  Avoids blocking the game thread with submit().get()
        // which can deadlock with Win32 SendMessage when the ultralight thread is
        // busy during renderer->Update().
        if (!ultralightThread.IsWorkerThread()) {
            return g_anyViewHasFocus.load(std::memory_order_acquire);
        }

        // On the ultralight thread: query directly for an accurate answer
        std::shared_lock lock(viewsMutex);
        for (const auto& pair : views) {
            if (pair.second && pair.second->ultralightView && pair.second->ultralightView->HasFocus()) {
                return true;
            }
        }
        return false;
    }

    void RegisterConsoleCallback(const Core::PrismaViewId& viewId,
                                 std::function<void(Core::PrismaViewId, PRISMA_UI_API::ConsoleMessageLevel, const std::string&)> callback) {
        std::unique_lock lock(viewsMutex);
        auto it = views.find(viewId);
        if (it != views.end() && it->second) {
            it->second->consoleMessageCallback = std::move(callback);
        } else {
            logger::warn("RegisterConsoleCallback: View ID [{}] not found.", viewId);
        }
    }
}
