#include "GPUDriverD3D11.h"

// Ultralight SDK pre-compiled shader bytecode headers
#include "d3d11/shaders.h"

namespace PrismaUI::GPU {

    GPUDriverD3D11::GPUDriverD3D11() {
        // Lightweight construction - D3D resources are initialized later via InitializeD3D()
    }

    void GPUDriverD3D11::InitializeD3D(ID3D11Device* device, ID3D11DeviceContext* context) {
        if (m_D3DInitialized) return;
        if (!device || !context) {
            logger::warn("GPUDriverD3D11::InitializeD3D: device or context is null.");
            return;
        }

        m_Device = device;
        m_Context = context;

        if (!LoadShaders()) {
            logger::critical("GPUDriverD3D11: Shader loading failed. Aborting initialization.");
            return;
        }

        if (!InitializeSamplerState()) {
            logger::critical("GPUDriverD3D11: Sampler state init failed. Aborting initialization.");
            return;
        }

        if (!InitializeBlendStates()) {
            logger::critical("GPUDriverD3D11: Blend state init failed. Aborting initialization.");
            return;
        }

        if (!InitializeRasterizerStates()) {
            logger::critical("GPUDriverD3D11: Rasterizer state init failed. Aborting initialization.");
            return;
        }

        if (!m_ConstantBuffer.Initialize(m_Device.Get())) {
            logger::critical("GPUDriverD3D11: Failed to initialize constant buffer.");
            return;
        }

        m_D3DInitialized = true;
        logger::info("GPUDriverD3D11: D3D resources initialized successfully.");
    }

    // ============================================================
    // ul::GPUDriver overrides - ID management
    // ============================================================

    void GPUDriverD3D11::BeginSynchronize() {
        // No-op: MSAA deferred
    }

    void GPUDriverD3D11::EndSynchronize() {
        // No-op: MSAA deferred
    }

    uint32_t GPUDriverD3D11::NextTextureId() { return m_NextTextureId.fetch_add(1); }

    uint32_t GPUDriverD3D11::NextRenderBufferId() { return m_NextRenderBufferId.fetch_add(1); }

    uint32_t GPUDriverD3D11::NextGeometryId() { return m_NextGeometryId.fetch_add(1); }

    // ============================================================
    // Command list management
    // ============================================================

    void GPUDriverD3D11::UpdateCommandList(const ultralight::CommandList& list) {
        if (list.size) {
            m_CommandList.resize(list.size);
            memcpy(m_CommandList.data(), list.commands, sizeof(ultralight::Command) * list.size);
        }
    }

    void GPUDriverD3D11::DrawCommandList() {
        if (m_CommandList.empty()) return;
        if (!m_D3DInitialized) return;

        m_CurrentlyBoundRenderTargetId = 0;

        for (auto& cmd : m_CommandList) {
            if (cmd.command_type == ultralight::CommandType::DrawGeometry)
                DrawGeometry(cmd.geometry_id, cmd.indices_count, cmd.indices_offset, cmd.gpu_state);
            else if (cmd.command_type == ultralight::CommandType::ClearRenderBuffer)
                ClearRenderBuffer(cmd.gpu_state.render_buffer_id);
        }

        m_CommandList.clear();
    }

    // ============================================================
    // Texture management
    // ============================================================

    void GPUDriverD3D11::CreateTexture(uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
        if (!m_D3DInitialized) {
            logger::error("GPUDriverD3D11::CreateTexture called before D3D initialization.");
            return;
        }

        if (m_TextureMap.find(textureId) != m_TextureMap.end()) {
            logger::error("GPUDriverD3D11::CreateTexture: texture id {} already exists.", textureId);
            return;
        }

        if (!(bitmap->format() == ultralight::BitmapFormat::BGRA8_UNORM_SRGB ||
              bitmap->format() == ultralight::BitmapFormat::A8_UNORM)) {
            logger::error("GPUDriverD3D11::CreateTexture: unsupported bitmap format.");
            return;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = bitmap->width();
        desc.Height = bitmap->height();
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = bitmap->format() == ultralight::BitmapFormat::BGRA8_UNORM_SRGB ? DXGI_FORMAT_B8G8R8A8_UNORM
                                                                                     : DXGI_FORMAT_A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags = 0;

        auto& textureEntry = m_TextureMap[textureId];
        HRESULT hr;

        if (bitmap->IsEmpty()) {
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.CPUAccessFlags = 0;
            hr = m_Device->CreateTexture2D(&desc, nullptr, &textureEntry.Texture);
        } else {
            D3D11_SUBRESOURCE_DATA textureData = {};
            textureData.pSysMem = bitmap->LockPixels();
            textureData.SysMemPitch = bitmap->row_bytes();
            textureData.SysMemSlicePitch = static_cast<UINT>(bitmap->size());
            hr = m_Device->CreateTexture2D(&desc, &textureData, &textureEntry.Texture);
            bitmap->UnlockPixels();
        }

        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11::CreateTexture: CreateTexture2D failed. HR={:#X}", hr);
            m_TextureMap.erase(textureId);
            return;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = m_Device->CreateShaderResourceView(textureEntry.Texture.Get(), &srvDesc, &textureEntry.TextureSRV);
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11::CreateTexture: CreateShaderResourceView failed. HR={:#X}", hr);
            m_TextureMap.erase(textureId);
            return;
        }
    }

    void GPUDriverD3D11::UpdateTexture(uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
        auto iter = m_TextureMap.find(textureId);
        if (iter == m_TextureMap.end()) {
            logger::error("GPUDriverD3D11::UpdateTexture: texture id {} doesn't exist.", textureId);
            return;
        }

        auto& entry = iter->second;
        D3D11_MAPPED_SUBRESOURCE res;
        HRESULT hr = m_Context->Map(entry.Texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
        if (FAILED(hr)) {
            logger::error("GPUDriverD3D11::UpdateTexture: Map failed. HR={:#X}", hr);
            return;
        }

        if (res.RowPitch == bitmap->row_bytes()) {
            memcpy(res.pData, bitmap->LockPixels(), bitmap->size());
            bitmap->UnlockPixels();
        } else {
            ultralight::RefPtr<ultralight::Bitmap> mapped_bitmap =
                ultralight::Bitmap::Create(bitmap->width(), bitmap->height(), bitmap->format(), res.RowPitch, res.pData,
                                           res.RowPitch * bitmap->height(), false);
            ultralight::IntRect dest_rect = {0, 0, (int)bitmap->width(), (int)bitmap->height()};
            mapped_bitmap->DrawBitmap(dest_rect, dest_rect, bitmap, false);
        }

        m_Context->Unmap(entry.Texture.Get(), 0);
    }

    void GPUDriverD3D11::DestroyTexture(uint32_t textureId) {
        auto iter = m_TextureMap.find(textureId);
        if (iter != m_TextureMap.end()) {
            m_TextureMap.erase(iter);
        } else {
            logger::warn("GPUDriverD3D11::DestroyTexture: texture id {} not found.", textureId);
        }
    }

    // ============================================================
    // Render buffer management
    // ============================================================

    void GPUDriverD3D11::CreateRenderBuffer(uint32_t renderBufferId, const ultralight::RenderBuffer& buffer) {
        if (!m_D3DInitialized) return;

        if (renderBufferId == 0) {
            logger::error("GPUDriverD3D11::CreateRenderBuffer: ID 0 is reserved.");
            return;
        }

        if (m_RenderTargetMap.find(renderBufferId) != m_RenderTargetMap.end()) {
            logger::error("GPUDriverD3D11::CreateRenderBuffer: ID {} already exists.", renderBufferId);
            return;
        }

        auto textureIter = m_TextureMap.find(buffer.texture_id);
        if (textureIter == m_TextureMap.end()) {
            logger::error("GPUDriverD3D11::CreateRenderBuffer: texture id {} doesn't exist.", buffer.texture_id);
            return;
        }

        auto& render_target_entry = m_RenderTargetMap[renderBufferId];
        render_target_entry.RenderTargetTextureId = buffer.texture_id;

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

        HRESULT hr = m_Device->CreateRenderTargetView(textureIter->second.Texture.Get(), &rtvDesc,
                                                      render_target_entry.RenderTargetView.GetAddressOf());
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11::CreateRenderBuffer: CreateRenderTargetView failed. HR={:#X}", hr);
            m_RenderTargetMap.erase(renderBufferId);
            return;
        }

        logger::debug("GPUDriverD3D11: Created render buffer {} for texture {}", renderBufferId, buffer.texture_id);
    }

    void GPUDriverD3D11::DestroyRenderBuffer(uint32_t renderBufferId) {
        auto iter = m_RenderTargetMap.find(renderBufferId);
        if (iter != m_RenderTargetMap.end()) {
            iter->second.RenderTargetView.Reset();
            m_RenderTargetMap.erase(iter);
            logger::debug("GPUDriverD3D11: Destroyed render buffer {}", renderBufferId);
        } else {
            logger::warn("GPUDriverD3D11::DestroyRenderBuffer: ID {} not found.", renderBufferId);
        }
    }

    // ============================================================
    // Geometry management
    // ============================================================

    void GPUDriverD3D11::CreateGeometry(uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                                        const ultralight::IndexBuffer& indices) {
        if (!m_D3DInitialized) return;

        if (m_GeometryMap.find(geometryId) != m_GeometryMap.end()) {
            logger::error("GPUDriverD3D11::CreateGeometry: ID {} already exists.", geometryId);
            return;
        }

        GeometryEntry geometry;
        geometry.Format = vertices.format;

        D3D11_BUFFER_DESC vertex_desc = {};
        vertex_desc.Usage = D3D11_USAGE_DYNAMIC;
        vertex_desc.ByteWidth = vertices.size;
        vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA vertex_data = {};
        vertex_data.pSysMem = vertices.data;

        HRESULT hr = m_Device->CreateBuffer(&vertex_desc, &vertex_data, geometry.VertexBuffer.GetAddressOf());
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11::CreateGeometry: CreateBuffer (vertex) failed. HR={:#X}", hr);
            return;
        }

        D3D11_BUFFER_DESC index_desc = {};
        index_desc.Usage = D3D11_USAGE_DYNAMIC;
        index_desc.ByteWidth = indices.size;
        index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        index_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA index_data = {};
        index_data.pSysMem = indices.data;

        hr = m_Device->CreateBuffer(&index_desc, &index_data, geometry.IndexBuffer.GetAddressOf());
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11::CreateGeometry: CreateBuffer (index) failed. HR={:#X}", hr);
            return;
        }

        m_GeometryMap.insert({geometryId, std::move(geometry)});
    }

    void GPUDriverD3D11::UpdateGeometry(uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                                        const ultralight::IndexBuffer& indices) {
        auto iter = m_GeometryMap.find(geometryId);
        if (iter == m_GeometryMap.end()) {
            logger::error("GPUDriverD3D11::UpdateGeometry: ID {} doesn't exist.", geometryId);
            return;
        }

        auto& entry = iter->second;
        D3D11_MAPPED_SUBRESOURCE res;

        HRESULT hr = m_Context->Map(entry.VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
        if (SUCCEEDED(hr)) {
            memcpy(res.pData, vertices.data, vertices.size);
            m_Context->Unmap(entry.VertexBuffer.Get(), 0);
        } else {
            logger::error("GPUDriverD3D11::UpdateGeometry: Failed to map vertex buffer for ID {}. HR={:#X}", geometryId, hr);
        }

        hr = m_Context->Map(entry.IndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
        if (SUCCEEDED(hr)) {
            memcpy(res.pData, indices.data, indices.size);
            m_Context->Unmap(entry.IndexBuffer.Get(), 0);
        } else {
            logger::error("GPUDriverD3D11::UpdateGeometry: Failed to map index buffer for ID {}. HR={:#X}", geometryId, hr);
        }
    }

    void GPUDriverD3D11::DestroyGeometry(uint32_t geometryId) {
        auto iter = m_GeometryMap.find(geometryId);
        if (iter != m_GeometryMap.end()) {
            iter->second.VertexBuffer.Reset();
            iter->second.IndexBuffer.Reset();
            m_GeometryMap.erase(iter);
        } else {
            logger::warn("GPUDriverD3D11::DestroyGeometry: ID {} not found.", geometryId);
        }
    }

    // ============================================================
    // Drawing
    // ============================================================

    void GPUDriverD3D11::DrawGeometry(uint32_t geometryId, uint32_t indexCount, uint32_t indexOffset,
                                      const ultralight::GPUState& state) {
        BindRenderBuffer(state.render_buffer_id);
        SetViewport(state.viewport_width, state.viewport_height);

        if (state.texture_1_id) BindTexture(0, state.texture_1_id);
        if (state.texture_2_id) BindTexture(1, state.texture_2_id);
        if (state.texture_3_id) BindTexture(2, state.texture_3_id);

        UpdateConstantBuffer(state);
        BindGeometry(geometryId);

        m_Context->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

        // Bind shaders based on shader type
        if (state.shader_type == ultralight::ShaderType::Fill) {
            m_Context->VSSetShader(m_VertexShader_Fill.GetShader(), nullptr, 0);
            m_Context->PSSetShader(m_PixelShader_Fill.GetShader(), nullptr, 0);
        } else if (state.shader_type == ultralight::ShaderType::FillPath) {
            m_Context->VSSetShader(m_VertexShader_FillPath.GetShader(), nullptr, 0);
            m_Context->PSSetShader(m_PixelShader_FillPath.GetShader(), nullptr, 0);
        } else if (state.shader_type == ultralight::ShaderType::FilterBasic) {
            m_Context->VSSetShader(m_VertexShader_Fill.GetShader(), nullptr, 0);
            m_Context->PSSetShader(m_PixelShader_FilterBasic.GetShader(), nullptr, 0);
        } else if (state.shader_type == ultralight::ShaderType::FilterBlur) {
            m_Context->VSSetShader(m_VertexShader_Fill.GetShader(), nullptr, 0);
            m_Context->PSSetShader(m_PixelShader_FilterBlur.GetShader(), nullptr, 0);
        } else if (state.shader_type == ultralight::ShaderType::FilterDropShadow) {
            m_Context->VSSetShader(m_VertexShader_Fill.GetShader(), nullptr, 0);
            m_Context->PSSetShader(m_PixelShader_FilterDropShadow.GetShader(), nullptr, 0);
        } else {
            logger::warn("GPUDriverD3D11::DrawGeometry: Unrecognized shader_type {}. Draw call skipped.",
                         static_cast<int>(state.shader_type));
            return;
        }

        // Blend state
        if (state.enable_blend)
            m_Context->OMSetBlendState(m_BlendState_Enabled.Get(), nullptr, 0xFFFFFFFF);
        else
            m_Context->OMSetBlendState(m_BlendState_Disabled.Get(), nullptr, 0xFFFFFFFF);

        // Rasterizer state
        if (state.enable_scissor) {
            m_Context->RSSetState(m_RasterizerState_Scissored.Get());
            D3D11_RECT scissor_rect = {(LONG)(state.scissor_rect.left), (LONG)(state.scissor_rect.top),
                                       (LONG)(state.scissor_rect.right), (LONG)(state.scissor_rect.bottom)};
            m_Context->RSSetScissorRects(1, &scissor_rect);
        } else {
            m_Context->RSSetState(m_RasterizerState_Default.Get());
        }

        // Constant buffers
        m_Context->VSSetConstantBuffers(0, 1, m_ConstantBuffer.GetAddressOf());
        m_Context->PSSetConstantBuffers(0, 1, m_ConstantBuffer.GetAddressOf());

        // Draw
        m_Context->DrawIndexed(indexCount, indexOffset, 0);
    }

    void GPUDriverD3D11::ClearRenderBuffer(uint32_t renderBufferId) {
        if (renderBufferId == 0) return;

        float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        auto iter = m_RenderTargetMap.find(renderBufferId);
        if (iter == m_RenderTargetMap.end()) {
            logger::error("GPUDriverD3D11::ClearRenderBuffer: ID {} not found.", renderBufferId);
            return;
        }

        m_Context->ClearRenderTargetView(iter->second.RenderTargetView.Get(), color);
    }

    // ============================================================
    // View texture access
    // ============================================================

    ID3D11ShaderResourceView* GPUDriverD3D11::GetShaderResourceView(ultralight::View* pView) {
        if (!pView) return nullptr;

        auto textureId = pView->render_target().texture_id;
        auto iter = m_TextureMap.find(textureId);
        if (iter == m_TextureMap.end()) return nullptr;

        return iter->second.TextureSRV.Get();
    }

    ID3D11Texture2D* GPUDriverD3D11::GetTexture(ultralight::View* pView) {
        if (!pView) return nullptr;

        auto textureId = pView->render_target().texture_id;
        auto iter = m_TextureMap.find(textureId);
        if (iter == m_TextureMap.end()) return nullptr;

        return iter->second.Texture.Get();
    }

    // ============================================================
    // Internal helpers
    // ============================================================

    void GPUDriverD3D11::BindRenderBuffer(uint32_t renderBufferId) {
        if (renderBufferId == m_CurrentlyBoundRenderTargetId) return;

        m_CurrentlyBoundRenderTargetId = renderBufferId;

        // Unbind shader resources to avoid warnings
        ID3D11ShaderResourceView* nullSRV[3] = {nullptr, nullptr, nullptr};
        m_Context->PSSetShaderResources(0, 3, nullSRV);

        if (renderBufferId == 0) return;

        auto iter = m_RenderTargetMap.find(renderBufferId);
        if (iter == m_RenderTargetMap.end()) {
            logger::error("GPUDriverD3D11::BindRenderBuffer: ID {} not found.", renderBufferId);
            return;
        }

        ID3D11RenderTargetView* target = iter->second.RenderTargetView.Get();
        m_Context->OMSetRenderTargets(1, &target, nullptr);
    }

    void GPUDriverD3D11::SetViewport(uint32_t width, uint32_t height) {
        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(width);
        vp.Height = static_cast<float>(height);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_Context->RSSetViewports(1, &vp);
    }

    void GPUDriverD3D11::BindTexture(uint8_t textureUnit, uint32_t textureId) {
        auto iter = m_TextureMap.find(textureId);
        if (iter == m_TextureMap.end()) {
            logger::error("GPUDriverD3D11::BindTexture: texture id {} not found.", textureId);
            return;
        }

        m_Context->PSSetShaderResources(textureUnit, 1, iter->second.TextureSRV.GetAddressOf());
    }

    void GPUDriverD3D11::UpdateConstantBuffer(const ultralight::GPUState& state) {
        float screenWidth = static_cast<float>(state.viewport_width);
        float screenHeight = static_cast<float>(state.viewport_height);
        ultralight::Matrix modelViewProjectionMat = ApplyProjection(state.transform, screenWidth, screenHeight);

        auto& cbdata = m_ConstantBuffer.m_Data;
        cbdata.State = {0.0f, screenWidth, screenHeight, 1.0f};
        cbdata.Transform = DirectX::XMMATRIX(modelViewProjectionMat.GetMatrix4x4().data);

        // Integer uniforms (SDK 1.4)
        cbdata.Integer4[0] = {state.uniform_integer[0], state.uniform_integer[1], state.uniform_integer[2],
                              state.uniform_integer[3]};
        cbdata.Integer4[1] = {state.uniform_integer[4], state.uniform_integer[5], state.uniform_integer[6],
                              state.uniform_integer[7]};

        cbdata.Scalar4[0] = {state.uniform_scalar[0], state.uniform_scalar[1], state.uniform_scalar[2],
                             state.uniform_scalar[3]};
        cbdata.Scalar4[1] = {state.uniform_scalar[4], state.uniform_scalar[5], state.uniform_scalar[6],
                             state.uniform_scalar[7]};

        // Use DirectXMath SIMD operations for vector conversion
        for (size_t i = 0; i < 8; ++i) {
            DirectX::XMVECTOR vec = DirectX::XMVectorSet(state.uniform_vector[i].x, state.uniform_vector[i].y,
                                                         state.uniform_vector[i].z, state.uniform_vector[i].w);
            DirectX::XMStoreFloat4(&cbdata.Vector[i], vec);
        }

        cbdata.ClipData = {static_cast<int32_t>(state.clip_size), 0, 0, 0};
        for (size_t i = 0; i < state.clip_size; ++i) cbdata.Clip[i] = DirectX::XMMATRIX(state.clip[i].data);

        m_ConstantBuffer.ApplyChanges(m_Context.Get());
    }

    void GPUDriverD3D11::BindGeometry(uint32_t geometryId) {
        auto iter = m_GeometryMap.find(geometryId);
        if (iter == m_GeometryMap.end()) {
            logger::error("GPUDriverD3D11::BindGeometry: ID {} not found.", geometryId);
            return;
        }

        auto& geometry = iter->second;
        UINT stride = geometry.Format == ultralight::VertexBufferFormat::_2f_4ub_2f
                          ? sizeof(ultralight::Vertex_2f_4ub_2f)
                          : sizeof(ultralight::Vertex_2f_4ub_2f_2f_28f);
        UINT offset = 0;

        m_Context->IASetVertexBuffers(0, 1, geometry.VertexBuffer.GetAddressOf(), &stride, &offset);
        m_Context->IASetIndexBuffer(geometry.IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        if (geometry.Format == ultralight::VertexBufferFormat::_2f_4ub_2f)
            m_Context->IASetInputLayout(m_VertexShader_FillPath.GetInputLayout());
        else
            m_Context->IASetInputLayout(m_VertexShader_Fill.GetInputLayout());
    }

    ultralight::Matrix GPUDriverD3D11::ApplyProjection(const ultralight::Matrix4x4& transform, float screenWidth,
                                                       float screenHeight) {
        ultralight::Matrix transformMatrix;
        transformMatrix.Set(transform);

        ultralight::Matrix result;
        result.SetOrthographicProjection(screenWidth, screenHeight, false);
        result.Transform(transformMatrix);

        return result;
    }

    // ============================================================
    // Initialization helpers
    // ============================================================

    bool GPUDriverD3D11::LoadShaders() {
        bool success = true;

        // Path vertex shader + path pixel shader
        if (!m_VertexShader_FillPath.Initialize(m_Device.Get(), {vertex_path_vs_data, vertex_path_vs_size},
                                                InputLayoutDescription::ultralight_2f_4ub_2f)) {
            logger::critical("GPUDriverD3D11: Failed to init FillPath vertex shader.");
            success = false;
        }

        if (!m_PixelShader_FillPath.Initialize(m_Device.Get(), {fill_path_ps_data, fill_path_ps_size})) {
            logger::critical("GPUDriverD3D11: Failed to init FillPath pixel shader.");
            success = false;
        }

        // Quad vertex shader + fill pixel shader
        if (!m_VertexShader_Fill.Initialize(m_Device.Get(), {vertex_quad_vs_data, vertex_quad_vs_size},
                                            InputLayoutDescription::ultralight_2f_4ub_2f_2f_28f)) {
            logger::critical("GPUDriverD3D11: Failed to init Fill vertex shader.");
            success = false;
        }

        if (!m_PixelShader_Fill.Initialize(m_Device.Get(), {fill_ps_data, fill_ps_size})) {
            logger::critical("GPUDriverD3D11: Failed to init Fill pixel shader.");
            success = false;
        }

        // Filter shaders
        if (!m_PixelShader_FilterBasic.Initialize(m_Device.Get(), {filter_basic_ps_data, filter_basic_ps_size})) {
            logger::critical("GPUDriverD3D11: Failed to init FilterBasic pixel shader.");
            success = false;
        }

        if (!m_PixelShader_FilterBlur.Initialize(m_Device.Get(), {filter_blur_ps_data, filter_blur_ps_size})) {
            logger::critical("GPUDriverD3D11: Failed to init FilterBlur pixel shader.");
            success = false;
        }

        if (!m_PixelShader_FilterDropShadow.Initialize(m_Device.Get(), {filter_dropshadow_ps_data,
                                                       filter_dropshadow_ps_size})) {
            logger::critical("GPUDriverD3D11: Failed to init FilterDropShadow pixel shader.");
            success = false;
        }

        if (success) logger::info("GPUDriverD3D11: All shaders loaded from SDK bytecode.");
        return success;
    }

    bool GPUDriverD3D11::InitializeSamplerState() {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MinLOD = 0;

        HRESULT hr = m_Device->CreateSamplerState(&desc, &m_SamplerState);
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11: Failed to create sampler state. HR={:#X}", hr);
            return false;
        }
        return true;
    }

    bool GPUDriverD3D11::InitializeBlendStates() {
        // Enabled blend state
        CD3D11_BLEND_DESC blendDescEnabled(D3D11_DEFAULT);
        blendDescEnabled.RenderTarget[0].BlendEnable = TRUE;
        blendDescEnabled.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDescEnabled.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
        blendDescEnabled.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;

        HRESULT hr = m_Device->CreateBlendState(&blendDescEnabled, &m_BlendState_Enabled);
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11: Failed to create enabled blend state. HR={:#X}", hr);
            return false;
        }

        // Disabled blend state
        CD3D11_BLEND_DESC blendDescDisabled(D3D11_DEFAULT);
        hr = m_Device->CreateBlendState(&blendDescDisabled, &m_BlendState_Disabled);
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11: Failed to create disabled blend state. HR={:#X}", hr);
            return false;
        }
        return true;
    }

    bool GPUDriverD3D11::InitializeRasterizerStates() {
        // Default rasterizer state (no scissor)
        CD3D11_RASTERIZER_DESC defaultDesc(D3D11_DEFAULT);
        defaultDesc.CullMode = D3D11_CULL_NONE;
        defaultDesc.DepthClipEnable = FALSE;

        HRESULT hr = m_Device->CreateRasterizerState(&defaultDesc, &m_RasterizerState_Default);
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11: Failed to create default rasterizer state. HR={:#X}", hr);
            return false;
        }

        // Scissored rasterizer state
        CD3D11_RASTERIZER_DESC scissorDesc(D3D11_DEFAULT);
        scissorDesc.CullMode = D3D11_CULL_NONE;
        scissorDesc.DepthClipEnable = FALSE;
        scissorDesc.ScissorEnable = TRUE;

        hr = m_Device->CreateRasterizerState(&scissorDesc, &m_RasterizerState_Scissored);
        if (FAILED(hr)) {
            logger::critical("GPUDriverD3D11: Failed to create scissored rasterizer state. HR={:#X}", hr);
            return false;
        }
        return true;
    }

}  // namespace PrismaUI::GPU
