#include "Inspector.h"

#include <algorithm>

#include "Core.h"
#include "ViewManager.h"

#ifdef min
    #undef min
#endif
#ifdef max
    #undef max
#endif

namespace PrismaUI::Inspector {
    using namespace Core;

    void CreateInspectorView(const PrismaViewId& viewId) {
        if (!AreInspectorAssetsAvailable()) {
            logger::warn(
                "View [{}]: Inspector assets were not found. Copy the Ultralight inspector folder next to PrismaUI.dll "
                "to enable the inspector.",
                viewId);
            return;
        }

        std::shared_ptr<PrismaView> viewData = nullptr;
        {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(viewId);
            if (it != views.end()) {
                viewData = it->second;
            }
        }

        if (!viewData) {
            logger::warn("CreateInspectorView: View ID [{}] not found.", viewId);
            return;
        }

        if (viewData->inspectorView) {
            logger::info("View [{}]: Inspector view already exists.", viewId);
            return;
        }

        if (!viewData->ultralightView) {
            logger::warn("View [{}]: Cannot create inspector because Ultralight view is not ready yet.", viewId);
            return;
        }

        try {
            auto createInspector = [view = viewData]() {
                if (view->ultralightView) {
                    view->ultralightView->CreateLocalInspectorView();
                }
            };

            if (ultralightThread.IsWorkerThread()) {
                createInspector();
            } else {
                auto future =
                    ultralightThread.submit_with_priority(SingleThreadExecutor::Priority::MEDIUM, createInspector);
                WaitWithMessagePump(future);
            }
            logger::info("View [{}]: Inspector creation requested.", viewId);
        } catch (const std::exception& e) {
            logger::error("View [{}]: Exception while creating inspector view: {}", viewId, e.what());
        }
    }

    void SetInspectorVisibility(const PrismaViewId& viewId, bool visible) {
        std::shared_ptr<PrismaView> viewData = nullptr;
        {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(viewId);
            if (it != views.end()) {
                viewData = it->second;
            }
        }

        if (!viewData) {
            logger::warn("SetInspectorVisibility: View ID [{}] not found.", viewId);
            return;
        }

        if (!viewData->inspectorView && visible) {
            CreateInspectorView(viewId);
        }

        if (!viewData->inspectorView) {
            logger::warn("View [{}]: Inspector view is not available to {}.", viewId, visible ? "show" : "hide");
            return;
        }

        viewData->inspectorVisible.store(visible);
        viewData->inspectorPointerHover.store(false);

        if (visible && viewData->inspectorView) {
            // Focus the inspector view when made visible
            auto focusInspector = [view = viewData]() {
                if (view->inspectorView) {
                    view->inspectorView->Focus();
                }
                if (view->ultralightView && view->ultralightView->HasFocus()) {
                    view->ultralightView->Unfocus();
                }
            };

            if (ultralightThread.IsWorkerThread()) {
                focusInspector();
            } else {
                auto future =
                    ultralightThread.submit_with_priority(SingleThreadExecutor::Priority::MEDIUM, focusInspector);
                WaitWithMessagePump(future);
            }
        }

        logger::info("View [{}]: Inspector visibility set to {}.", viewId, visible);
    }

    bool IsInspectorVisible(const PrismaViewId& viewId) {
        std::shared_lock lock(viewsMutex);
        auto it = views.find(viewId);
        if (it != views.end() && it->second) {
            return it->second->inspectorVisible.load();
        }
        return false;
    }

    void SetInspectorBounds(const PrismaViewId& viewId, float topLeftX, float topLeftY, uint32_t width,
                            uint32_t height) {
        width = std::max<uint32_t>(width, 32u);
        height = std::max<uint32_t>(height, 32u);

        std::shared_ptr<PrismaView> viewData = nullptr;
        {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(viewId);
            if (it != views.end()) {
                viewData = it->second;
            }
        }

        if (!viewData) {
            logger::warn("SetInspectorBounds: View ID [{}] not found.", viewId);
            return;
        }

        if (!viewData->inspectorView) {
            logger::warn("View [{}]: Cannot set inspector bounds because inspector is not available.", viewId);
            return;
        }

        const float screenW = static_cast<float>(screenSize.width ? screenSize.width : width);
        const float screenH = static_cast<float>(screenSize.height ? screenSize.height : height);
        const float maxX = std::max(0.0f, screenW - static_cast<float>(width));
        const float maxY = std::max(0.0f, screenH - static_cast<float>(height));

        viewData->inspectorPosX.store(std::clamp(topLeftX, 0.0f, maxX), std::memory_order_relaxed);
        viewData->inspectorPosY.store(std::clamp(topLeftY, 0.0f, maxY), std::memory_order_relaxed);
        viewData->inspectorDisplayWidth.store(width, std::memory_order_relaxed);
        viewData->inspectorDisplayHeight.store(height, std::memory_order_relaxed);
        viewData->inspectorPointerHover.store(false);

        try {
            auto resizeInspector = [view = viewData, width, height]() {
                if (view->inspectorView) {
                    view->inspectorView->Resize(width, height);
                }
            };

            if (ultralightThread.IsWorkerThread()) {
                resizeInspector();
            } else {
                auto future =
                    ultralightThread.submit_with_priority(SingleThreadExecutor::Priority::MEDIUM, resizeInspector);
                WaitWithMessagePump(future);
            }
            logger::info("View [{}]: Inspector bounds set to ({}, {}) with size {}x{}", viewId, viewData->inspectorPosX.load(),
                         viewData->inspectorPosY.load(), width, height);
        } catch (const std::exception& e) {
            logger::error("View [{}]: Exception while setting inspector bounds: {}", viewId, e.what());
        }
    }

}  // namespace PrismaUI::Inspector
