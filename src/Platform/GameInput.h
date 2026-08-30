#pragma once

#include <cstdint>

namespace PrismaUI::Platform::GameInput {
    enum class NotifyControl : std::uint32_t { kContinue, kStop };

    enum class Device : std::uint32_t { kKeyboard, kMouse, kGamepad };

    enum class EventType : std::uint32_t { kButton, kMouseMove, kChar, kThumbstick };

    struct InputEvent {
        void* vtable;
        Device device;
        EventType eventType;
        InputEvent* next;
    };
    static_assert(sizeof(InputEvent) == 0x18);

    struct ButtonEvent {
        [[nodiscard]] std::uint32_t IdCode() const noexcept;
        [[nodiscard]] Device InputDevice() const noexcept;
        [[nodiscard]] float Value() const noexcept;
        [[nodiscard]] float HeldDuration() const noexcept;
        [[nodiscard]] bool IsDown() const noexcept;
        [[nodiscard]] bool IsUp() const noexcept;
    };

    struct ThumbstickEvent {
        [[nodiscard]] bool IsLeft() const noexcept;
        [[nodiscard]] bool IsRight() const noexcept;
        [[nodiscard]] float X() const noexcept;
        [[nodiscard]] float Y() const noexcept;
    };

    class Sink {
    public:
        virtual ~Sink() = default;
        virtual NotifyControl ProcessEvent(InputEvent* const* event, void* eventSource) = 0;
    };

    [[nodiscard]] bool AddSink(Sink* sink) noexcept;
    void RemoveSink(Sink* sink) noexcept;
}
