#include "Platform/GameServices.h"

#include <Windows.h>

#include <cstring>
#include <limits>

#include "Platform/Runtime.h"


namespace PrismaUI::Platform::GameServices {
    namespace {
        struct RawArray {
            void* data;
            std::uint32_t capacity;
            std::uint32_t pad0C;
            std::uint32_t size;
            std::uint32_t pad14;
        };
        static_assert(sizeof(RawArray) == 0x18);

        struct UserEventMapping {
            const char* eventId;
            std::uint16_t inputKey;
            std::uint16_t modifier;
            std::int8_t indexInContext;
            bool remappable;
            bool linked;
            std::uint8_t pad0F;
            std::uint32_t group;
            std::uint32_t pad14;
        };
        static_assert(sizeof(UserEventMapping) == 0x18);

        std::uintptr_t Resolved(AddressKey key) noexcept {
            if (!IsRuntimeInitialized()) {
                return 0;
            }
            return GetRelocations().Address(key).value_or(0);
        }

        void* Singleton(AddressKey key) noexcept {
            const auto address = Resolved(key);
            return address ? *reinterpret_cast<void**>(address) : nullptr;
        }
    }

    std::optional<GraphicsState> GetGraphicsState() noexcept {
        const auto renderer = Resolved(AddressKey::kRenderer);
        const auto graphicsState = Resolved(AddressKey::kGraphicsState);
        if (!renderer || !graphicsState) {
            return std::nullopt;
        }

        const auto dataOffset = GetRuntimeContext().Family() == RuntimeFamily::kVR ? 0x18U : 0x10U;
        const auto data = renderer + dataOffset;
        GraphicsState result;
        result.device = *reinterpret_cast<ID3D11Device**>(data + 0x38);
        result.context = *reinterpret_cast<ID3D11DeviceContext**>(data + 0x40);
        result.window = *reinterpret_cast<HWND*>(data + 0x48);
        result.screenSize.width = *reinterpret_cast<const std::uint32_t*>(graphicsState + 0x24);
        result.screenSize.height = *reinterpret_cast<const std::uint32_t*>(graphicsState + 0x28);
        if (!result.device || !result.context || !result.window || result.screenSize.width == 0 ||
            result.screenSize.height == 0) {
            return std::nullopt;
        }
        return result;
    }

    bool ToggleControl(Control control, bool enabled) noexcept {
        auto* controlMap = Singleton(AddressKey::kControlMap);
        const auto functionAddress = Resolved(AddressKey::kToggleControls);
        if (!controlMap || !functionAddress) {
            return false;
        }
        using ToggleControls = void (*)(void*, std::uint32_t, bool, bool);
        reinterpret_cast<ToggleControls>(functionAddress)(controlMap, static_cast<std::uint32_t>(control), enabled,
                                                          false);
        return true;
    }

    std::uint32_t GetMenuGamepadMapping(std::string_view eventName) noexcept {
        constexpr std::uint32_t kInvalid = std::numeric_limits<std::uint32_t>::max();
        constexpr std::size_t kControlMapOffset = 0x60;
        constexpr std::size_t kMenuContext = 1;
        constexpr std::size_t kGamepadDevice = 2;

        const auto* controlMap = static_cast<const std::byte*>(Singleton(AddressKey::kControlMap));
        if (!controlMap) {
            return kInvalid;
        }
        const auto contexts = reinterpret_cast<void* const*>(controlMap + kControlMapOffset);
        const auto* menuContext = static_cast<const std::byte*>(contexts[kMenuContext]);
        if (!menuContext) {
            return kInvalid;
        }
        const auto& mappings = *reinterpret_cast<const RawArray*>(menuContext + kGamepadDevice * sizeof(RawArray));
        if (!mappings.data || mappings.size > mappings.capacity || mappings.size > 4096) {
            return kInvalid;
        }
        const auto* entries = static_cast<const UserEventMapping*>(mappings.data);
        for (std::uint32_t index = 0; index < mappings.size; ++index) {
            if (entries[index].eventId && eventName == entries[index].eventId) {
                return entries[index].inputKey;
            }
        }
        return kInvalid;
    }

    bool IncrementPauseCount() noexcept {
        auto* ui = static_cast<std::byte*>(Singleton(AddressKey::kUI));
        if (!ui) {
            return false;
        }
        auto* count = reinterpret_cast<volatile LONG*>(ui + 0x160);
        for (;;) {
            const auto current = static_cast<std::uint32_t>(InterlockedCompareExchange(count, 0, 0));
            if (current == std::numeric_limits<std::uint32_t>::max()) {
                return false;
            }
            const auto desired = static_cast<LONG>(current + 1);
            if (static_cast<std::uint32_t>(InterlockedCompareExchange(count, desired, static_cast<LONG>(current))) ==
                current) {
                return true;
            }
        }
    }

    void DecrementPauseCount() noexcept {
        auto* ui = static_cast<std::byte*>(Singleton(AddressKey::kUI));
        if (!ui) {
            return;
        }
        auto* count = reinterpret_cast<volatile LONG*>(ui + 0x160);
        for (;;) {
            const auto current = static_cast<std::uint32_t>(InterlockedCompareExchange(count, 0, 0));
            if (current == 0) {
                return;
            }
            const auto desired = static_cast<LONG>(current - 1);
            if (static_cast<std::uint32_t>(InterlockedCompareExchange(count, desired, static_cast<LONG>(current))) ==
                current) {
                return;
            }
        }
    }
}
