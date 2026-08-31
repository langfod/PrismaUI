#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct HWND__;
using HWND = HWND__*;

namespace PrismaUI::Platform::GameServices {
    struct ScreenSize {
        std::uint32_t width{0};
        std::uint32_t height{0};
    };

    struct GraphicsState {
        ID3D11Device* device{nullptr};
        ID3D11DeviceContext* context{nullptr};
        HWND window{nullptr};
        ScreenSize screenSize;
    };

    enum class Control : std::uint32_t {
        kMovement = 1U << 0,
        kLooking = 1U << 1,
        kActivate = 1U << 2,
        kPOVSwitch = 1U << 5,
        kFighting = 1U << 6,
        kWheelZoom = 1U << 9,
        kJumping = 1U << 10,
        kVATS = 1U << 11
    };

    [[nodiscard]] std::optional<GraphicsState> GetGraphicsState() noexcept;
    bool ToggleControl(Control control, bool enabled) noexcept;
    [[nodiscard]] std::uint32_t GetMenuGamepadMapping(std::string_view eventName) noexcept;
    [[nodiscard]] bool IncrementPauseCount() noexcept;
    void DecrementPauseCount() noexcept;
}
