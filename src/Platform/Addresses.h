#pragma once

#include <cstdint>
#include <string_view>

namespace PrismaUI::Platform {
    enum class AddressKey : std::uint8_t {
        kD3DPresentCall,
        kRenderer,
        kGraphicsState,
        kUI,
        kControlMap,
        kToggleControls,
        kInputDeviceManager,
        kMemoryManager,
        kMemoryAllocate,
        kMemoryReallocate
    };

    struct AddressSpec {
        AddressKey key;
        std::string_view name;
        std::uint64_t seId;
        std::uint64_t aeId;
        std::uint64_t vrId;
        std::uint32_t seAddend;
        std::uint32_t aeAddend;
        std::uint32_t vrAddend;
        std::string_view expectedSection;
        bool indirect;
    };

    inline constexpr AddressSpec kAddressCatalog[] = {
        {AddressKey::kD3DPresentCall, "D3D present call", 75461, 77246, 75461, 0x9, 0x9, 0x15, ".text", false},
        {AddressKey::kRenderer, "BSGraphics::Renderer", 524907, 411393, 524907, 0, 0, 0, ".data", false},
        {AddressKey::kGraphicsState, "BSGraphics::State", 524998, 411479, 524998, 0, 0, 0, ".data", false},
        {AddressKey::kUI, "UI singleton", 514178, 400327, 514178, 0, 0, 0, ".data", true},
        {AddressKey::kControlMap, "ControlMap singleton", 514705, 400863, 514705, 0, 0, 0, ".data", true},
        {AddressKey::kToggleControls, "ControlMap::ToggleControls", 67245, 68545, 67245, 0, 0, 0, ".text", false},
        {AddressKey::kInputDeviceManager, "BSInputDeviceManager singleton", 516574, 402776, 516574, 0, 0, 0, ".data", true},
        {AddressKey::kMemoryManager, "MemoryManager::GetSingleton", 11045, 11141, 11045, 0, 0, 0, ".text", false},
        {AddressKey::kMemoryAllocate, "MemoryManager::Allocate", 66859, 68115, 66859, 0, 0, 0, ".text", false},
        {AddressKey::kMemoryReallocate, "MemoryManager::Reallocate", 66860, 68116, 66860, 0, 0, 0, ".text", false},
    };
}
