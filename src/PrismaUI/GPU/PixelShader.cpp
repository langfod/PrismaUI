#include "PixelShader.h"

namespace PrismaUI::GPU {

    bool PixelShader::Initialize(ID3D11Device* device, std::span<const unsigned char> bytecode) {
        if (!device || bytecode.empty()) return false;

        HRESULT hr = device->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, &m_Shader);
        if (FAILED(hr)) {
            logger::error("Failed to create pixel shader! HR={:#X}", static_cast<unsigned long>(hr));
            return false;
        }

        return true;
    }

    ID3D11PixelShader* PixelShader::GetShader() const { return m_Shader.Get(); }

}  // namespace PrismaUI::GPU
