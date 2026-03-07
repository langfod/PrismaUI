#include "Inspector.h"

#include "Core.h"
#include "Utils/SIMDDispatch.h"

namespace PrismaUI::Inspector {
    // BGRA pixel format constant
    constexpr uint32_t BYTES_PER_PIXEL = 4;
    using namespace Core;

    void RenderInspectorView(std::shared_ptr<PrismaView> viewData) {
        if (!viewData || !viewData->inspectorView || !viewData->inspectorVisible.load() || viewData->isHidden.load()) {
            return;
        }

        Surface* surface = viewData->inspectorView->surface();
        if (surface) {
            CopyInspectorBitmapToBuffer(viewData);
        }
    }

    void CopyInspectorBitmapToBuffer(std::shared_ptr<PrismaView> viewData) {
        if (!viewData || !viewData->inspectorView) {
            return;
        }

        Surface* surface = viewData->inspectorView->surface();
        if (!surface) {
            return;
        }

        // Safe to cast: we use the default BitmapSurfaceFactory, so surface is always a BitmapSurface
        BitmapSurface* bitmapSurface = static_cast<BitmapSurface*>(surface);

        RefPtr<Bitmap> bitmap = bitmapSurface->bitmap();
        if (!bitmap || bitmap->IsEmpty()) {
            return;
        }

        const void* pixels = bitmap->LockPixels();
        if (!pixels) {
            return;
        }

        const uint32_t width = bitmap->width();
        const uint32_t height = bitmap->height();
        const uint32_t stride = bitmap->row_bytes();
        const size_t dataSize = stride * height;

        {
            std::lock_guard<std::mutex> lock(viewData->inspectorBufferMutex);

            if (viewData->inspectorPixelBuffer.size() != dataSize) {
                viewData->inspectorPixelBuffer.resize(dataSize);
            }

            // Use SIMD-optimized memcpy
            SIMD::FastMemcpy(viewData->inspectorPixelBuffer.data(), pixels, dataSize);

            viewData->inspectorBufferWidth = width;
            viewData->inspectorBufferHeight = height;
            viewData->inspectorBufferStride = stride;
            viewData->inspectorFrameReady.store(true);
        }

        bitmap->UnlockPixels();
    }

    void CopyInspectorPixelsToTexture(PrismaView* viewData, void* pixels, uint32_t width, uint32_t height,
                                      uint32_t stride) {
        if (!viewData || !pixels || !d3dContext || !d3dDevice) {
            return;
        }

        if (width == 0 || height == 0) {
            return;
        }

        // Recreate texture if size changed
        if (!viewData->inspectorTexture || viewData->inspectorTextureWidth != width ||
            viewData->inspectorTextureHeight != height) {
            if (viewData->inspectorTextureView) {
                viewData->inspectorTextureView->Release();
                viewData->inspectorTextureView = nullptr;
            }
            if (viewData->inspectorTexture) {
                viewData->inspectorTexture->Release();
                viewData->inspectorTexture = nullptr;
            }

            D3D11_TEXTURE2D_DESC texDesc = {};
            texDesc.Width = width;
            texDesc.Height = height;
            texDesc.MipLevels = 1;
            texDesc.ArraySize = 1;
            texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            texDesc.SampleDesc.Count = 1;
            texDesc.Usage = D3D11_USAGE_DYNAMIC;
            texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            HRESULT hr = d3dDevice->CreateTexture2D(&texDesc, nullptr, &viewData->inspectorTexture);
            if (FAILED(hr)) {
                logger::error("Failed to create inspector D3D11 texture for View [{}]: HRESULT={:X}", viewData->id,
                              static_cast<unsigned>(hr));
                return;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = texDesc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;

            hr = d3dDevice->CreateShaderResourceView(viewData->inspectorTexture, &srvDesc,
                                                     &viewData->inspectorTextureView);
            if (FAILED(hr)) {
                logger::error("Failed to create inspector shader resource view for View [{}]: HRESULT={:X}",
                              viewData->id, static_cast<unsigned>(hr));
                viewData->inspectorTexture->Release();
                viewData->inspectorTexture = nullptr;
                return;
            }

            viewData->inspectorTextureWidth = width;
            viewData->inspectorTextureHeight = height;
        }

        // Upload pixels to texture
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = d3dContext->Map(viewData->inspectorTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            // Use SIMD-optimized pixel copying
            SIMD::CopyPixels(mapped.pData, mapped.RowPitch, pixels, stride, width, height);

            d3dContext->Unmap(viewData->inspectorTexture, 0);
        } else {
            logger::error("Failed to map inspector texture for View [{}]: HRESULT={:X}", viewData->id,
                          static_cast<unsigned>(hr));
        }
    }

}  // namespace PrismaUI::Inspector
