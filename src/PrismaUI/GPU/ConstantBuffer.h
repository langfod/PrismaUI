#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include <cstring>
#include <type_traits>

namespace PrismaUI::GPU {

    template <class T>
    class ConstantBuffer {
        static_assert(std::is_trivially_copyable_v<T>, "ConstantBuffer<T> requires T to be trivially copyable.");

    public:
        T m_Data{};

        ID3D11Buffer* Get() const { return m_Buffer.Get(); }
        ID3D11Buffer* const* GetAddressOf() const { return m_Buffer.GetAddressOf(); }

        bool Initialize(ID3D11Device* device) {
            if (!device) return false;

            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;
            desc.StructureByteStride = 0;

            desc.ByteWidth = static_cast<UINT>((sizeof(T) + 15) & ~static_cast<size_t>(15));

            HRESULT hr = device->CreateBuffer(&desc, nullptr, &m_Buffer);
            return SUCCEEDED(hr);
        }

        bool ApplyChanges(ID3D11DeviceContext* context) {
            if (!context || !m_Buffer) return false;

            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = context->Map(m_Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            if (FAILED(hr)) return false;

            memcpy(mappedResource.pData, &m_Data, sizeof(T));
            context->Unmap(m_Buffer.Get(), 0);
            return true;
        }

    private:
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_Buffer;
    };

}  // namespace PrismaUI::GPU
