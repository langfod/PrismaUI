#include "VertexShader.h"

namespace PrismaUI::GPU {

    bool VertexShader::Initialize(ID3D11Device* device, std::span<const unsigned char> bytecode,
                                  std::span<const D3D11_INPUT_ELEMENT_DESC> layoutDesc) {
        if (!device || bytecode.empty() || layoutDesc.empty()) return false;

        HRESULT hr = device->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &m_Shader);
        if (FAILED(hr)) {
            logger::error("Failed to create vertex shader! HR={:#X}", hr);
            return false;
        }

        hr = device->CreateInputLayout(layoutDesc.data(), static_cast<UINT>(layoutDesc.size()), bytecode.data(), bytecode.size(),
                                       &m_InputLayout);
        if (FAILED(hr)) {
            logger::error("Failed to create input layout for vertex shader! HR={:#X}", hr);
            m_Shader.Reset();
            return false;
        }

        return true;
    }

    ID3D11VertexShader* VertexShader::GetShader() const { return m_Shader.Get(); }

    ID3D11InputLayout* VertexShader::GetInputLayout() const { return m_InputLayout.Get(); }

}  // namespace PrismaUI::GPU
