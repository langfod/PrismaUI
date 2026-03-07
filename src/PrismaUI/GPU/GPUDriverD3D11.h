#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#include <Ultralight/platform/GPUDriver.h>
#pragma warning(pop)

#include "ConstantBuffer.h"
#include "GPUTypes.h"
#include "PixelShader.h"
#include "VertexShader.h"

namespace PrismaUI::GPU {

    class GPUDriverD3D11 : public ultralight::GPUDriver {
    public:
        GPUDriverD3D11();

        GPUDriverD3D11(const GPUDriverD3D11&) = delete;
        GPUDriverD3D11& operator=(const GPUDriverD3D11&) = delete;
        GPUDriverD3D11(GPUDriverD3D11&&) = delete;
        GPUDriverD3D11& operator=(GPUDriverD3D11&&) = delete;

        // Phase 2: Initialize D3D resources when device/context become available
        void InitializeD3D(ID3D11Device* device, ID3D11DeviceContext* context);
        bool IsD3DInitialized() const { return m_D3DInitialized; }
        bool HasPendingCommands() const { return m_hasPendingCommands.load(std::memory_order_acquire); }

        // ul::GPUDriver overrides
        void BeginSynchronize() override;
        void EndSynchronize() override;
        uint32_t NextTextureId() override;
        uint32_t NextRenderBufferId() override;
        uint32_t NextGeometryId() override;
        void UpdateCommandList(const ultralight::CommandList& list) override;

        void CreateTexture(uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) override;
        void UpdateTexture(uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) override;
        void DestroyTexture(uint32_t textureId) override;
        void CreateRenderBuffer(uint32_t renderBufferId, const ultralight::RenderBuffer& buffer) override;
        void DestroyRenderBuffer(uint32_t renderBufferId) override;
        void CreateGeometry(uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                            const ultralight::IndexBuffer& indices) override;
        void UpdateGeometry(uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                            const ultralight::IndexBuffer& indices) override;
        void DestroyGeometry(uint32_t geometryId) override;

        // Execute queued GPU commands
        void DrawCommandList();

        // Get the SRV for a view's render target (for compositing via SpriteBatch)
        ID3D11ShaderResourceView* GetShaderResourceView(ultralight::View* pView);

        // Get the raw texture for a view's render target
        ID3D11Texture2D* GetTexture(ultralight::View* pView);

    private:
        bool LoadShaders();
        bool InitializeSamplerState();
        bool InitializeBlendStates();
        bool InitializeRasterizerStates();

        void DrawGeometry(uint32_t geometryId, uint32_t indexCount, uint32_t indexOffset,
                          const ultralight::GPUState& state);
        void ClearRenderBuffer(uint32_t renderBufferId);
        void BindRenderBuffer(uint32_t renderBufferId);
        void SetViewport(uint32_t width, uint32_t height);
        void BindTexture(uint8_t textureUnit, uint32_t textureId);
        void UpdateConstantBuffer(const ultralight::GPUState& state);
        void BindGeometry(uint32_t geometryId);
        ultralight::Matrix ApplyProjection(const ultralight::Matrix4x4& transform, float screenWidth,
                                           float screenHeight);

        // D3D device/context (prevent dangling pointers by adding a reference)
        Microsoft::WRL::ComPtr<ID3D11Device> m_Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_Context;
        bool m_D3DInitialized = false;

        // ID counters
        std::atomic<uint32_t> m_NextTextureId{1};
        std::atomic<uint32_t> m_NextRenderBufferId{1};
        std::atomic<uint32_t> m_NextGeometryId{1};

        // Resource maps
        std::unordered_map<uint32_t, GeometryEntry> m_GeometryMap;
        std::unordered_map<uint32_t, RenderTargetEntry> m_RenderTargetMap;
        std::unordered_map<uint32_t, TextureEntry> m_TextureMap;

        // Command list
        std::vector<ultralight::Command> m_CommandList;
        std::atomic<bool> m_hasPendingCommands{false};

        // Shaders
        VertexShader m_VertexShader_Fill;
        VertexShader m_VertexShader_FillPath;
        PixelShader m_PixelShader_Fill;
        PixelShader m_PixelShader_FillPath;
        PixelShader m_PixelShader_FilterBasic;
        PixelShader m_PixelShader_FilterBlur;
        PixelShader m_PixelShader_FilterDropShadow;

        // Constant buffer
        ConstantBuffer<CB_UltralightData> m_ConstantBuffer;

        // Pipeline states
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_SamplerState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_BlendState_Disabled;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_BlendState_Enabled;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_RasterizerState_Default;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_RasterizerState_Scissored;

        // Render state tracking
        uint32_t m_CurrentlyBoundRenderTargetId = 0;
    };

}  // namespace PrismaUI::GPU
