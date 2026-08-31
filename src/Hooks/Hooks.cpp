#include "Hooks.h"

#include "Platform/Patching.h"
#include "Platform/Runtime.h"

namespace Hooks {
    void D3DPresentHook::Install(D3DPresentFunc* func, std::atomic<D3DPresentFunc*>& original)
    {
        const auto address = PrismaUI::Platform::GetRelocations().Address(
            PrismaUI::Platform::AddressKey::kD3DPresentCall);
        if (!address) {
            throw std::runtime_error("D3D present call relocation is unavailable");
        }

        std::string error;
        const auto prepared = PrismaUI::Platform::Patching::PrepareCall5(
            *address, reinterpret_cast<std::uintptr_t>(func), error);
        if (!prepared) {
            throw std::runtime_error(error);
        }

        original.store(reinterpret_cast<D3DPresentFunc*>(prepared->originalTarget), std::memory_order_release);
        if (!PrismaUI::Platform::Patching::ApplyCall5(*prepared, error)) {
            original.store(nullptr, std::memory_order_release);
            throw std::runtime_error(error);
        }
    }
}
