#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <span>

namespace PrismaUI::GPU {

    class PixelShader {
    public:
        PixelShader() = default;
        PixelShader(const PixelShader&) = delete;
        PixelShader& operator=(const PixelShader&) = delete;
        PixelShader(PixelShader&&) = delete;
        PixelShader& operator=(PixelShader&&) = delete;

        [[nodiscard]] bool Initialize(ID3D11Device* device, std::span<const unsigned char> bytecode);
        ID3D11PixelShader* GetShader() const;

    private:
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_Shader;
    };

}  // namespace PrismaUI::GPU
