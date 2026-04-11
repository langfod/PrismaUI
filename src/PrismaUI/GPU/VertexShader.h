#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <span>

namespace PrismaUI::GPU {

    class VertexShader {
    public:
        VertexShader() = default;
        VertexShader(const VertexShader&) = delete;
        VertexShader& operator=(const VertexShader&) = delete;
        VertexShader(VertexShader&&) = delete;
        VertexShader& operator=(VertexShader&&) = delete;

        [[nodiscard]] bool Initialize(ID3D11Device* device, std::span<const unsigned char> bytecode,
                        std::span<const D3D11_INPUT_ELEMENT_DESC> layoutDesc);
        ID3D11VertexShader* GetShader() const;
        ID3D11InputLayout* GetInputLayout() const;

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_Shader;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_InputLayout;
    };

}  // namespace PrismaUI::GPU
