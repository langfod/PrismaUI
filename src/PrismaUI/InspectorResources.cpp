#include "Inspector.h"

#include "Core.h"

namespace PrismaUI::Inspector {
    using namespace Core;

    void ReleaseInspectorTexture(PrismaView* viewData) {
        if (!viewData) {
            return;
        }

        if (viewData->inspectorTextureView) {
            viewData->inspectorTextureView->Release();
            viewData->inspectorTextureView = nullptr;
        }

        if (viewData->inspectorTexture) {
            viewData->inspectorTexture->Release();
            viewData->inspectorTexture = nullptr;
        }

        viewData->inspectorTextureWidth = 0;
        viewData->inspectorTextureHeight = 0;
    }

    void DestroyInspectorResources(PrismaView* viewData) {
        if (!viewData) {
            return;
        }

        {
            std::lock_guard bufferLock(viewData->inspectorBufferMutex);
            ReleaseInspectorTexture(viewData);
            viewData->inspectorFrameReady.store(false);
            viewData->inspectorPixelBuffer.clear();
            viewData->inspectorPixelBuffer.shrink_to_fit();
            viewData->inspectorBufferWidth = 0;
            viewData->inspectorBufferHeight = 0;
            viewData->inspectorBufferStride = 0;
        }

        viewData->inspectorPointerHover.store(false);
    }

}  // namespace PrismaUI::Inspector
