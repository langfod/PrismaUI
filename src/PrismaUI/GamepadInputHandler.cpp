#include "GamepadInputHandler.h"

#pragma warning(push)
#pragma warning(disable : 4100)
#include <Ultralight/Ultralight.h>
#pragma warning(pop)

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "Communication.h"
#include "Core.h"
#include "InputHandler.h"
#include "Platform/GameInput.h"
#include "Platform/GameServices.h"
#include "PrismaVR.h"
#include "Utils/SingleThreadExecutor.h"

namespace PrismaUI::GamepadInputHandler {
    namespace {
        constexpr std::uint32_t kGamepadIndex = 0;
        constexpr std::uint32_t kAxisCount = 4;
        constexpr std::uint32_t kButtonCount = 17;

        struct JsButtonDispatch {
            Core::PrismaViewId viewId;
            std::string script;
        };

        using QueuedEvent =
            std::variant<ultralight::GamepadButtonEvent, ultralight::GamepadAxisEvent, JsButtonDispatch>;
        std::mutex g_queueMutex;
        std::vector<QueuedEvent> g_queue;
        SingleThreadExecutor* g_ultralightThreadExecutor = nullptr;
        std::atomic_bool g_registered{false};
        std::atomic_uint64_t g_session{0};
        std::array<std::atomic<float>, kButtonCount> g_buttonValues{};

        int SkyrimCodeToW3C(std::uint32_t code) noexcept {
            switch (code) {
                case 0x1000: return 0;
                case 0x2000: return 1;
                case 0x4000: return 2;
                case 0x8000: return 3;
                case 0x0100: return 4;
                case 0x0200: return 5;
                case 0x0009: return 6;
                case 0x000A: return 7;
                case 0x0020: return 8;
                case 0x0010: return 9;
                case 0x0040: return 10;
                case 0x0080: return 11;
                case 0x0001: return 12;
                case 0x0002: return 13;
                case 0x0004: return 14;
                case 0x0008: return 15;
                default: return -1;
            }
        }

        std::optional<JsButtonDispatch> BuildDispatch(const char* eventName, std::uint32_t w3cIndex,
                                                      std::uint32_t skyrimCode) {
            const auto viewId = InputHandler::GetFocusedViewId();
            if (viewId == 0) {
                return std::nullopt;
            }
            const auto accept = Platform::GameServices::GetMenuGamepadMapping("Accept");
            const auto cancel = Platform::GameServices::GetMenuGamepadMapping("Cancel");
            const char* action = skyrimCode == accept ? "accept" : skyrimCode == cancel ? "cancel" : "";
            auto script = std::string("window.dispatchEvent(new CustomEvent(\"") + eventName +
                          "\", {detail: {w3cButtonIndex: " + std::to_string(w3cIndex) +
                          ", skyrimIdCode: " + std::to_string(skyrimCode) + ", action: \"" + action + "\"}}))";
            return JsButtonDispatch{viewId, std::move(script)};
        }

        void QueueButton(const Platform::GameInput::ButtonEvent* button) {
            const auto skyrimCode = button->IdCode();
            const auto w3cIndexValue = SkyrimCodeToW3C(skyrimCode);
            if (w3cIndexValue < 0) {
                return;
            }
            const auto w3cIndex = static_cast<std::uint32_t>(w3cIndexValue);
            std::optional<JsButtonDispatch> dispatch;
            if (button->IsDown()) {
                dispatch = BuildDispatch("prismagamepadbuttondown", w3cIndex, skyrimCode);
            } else if (button->IsUp()) {
                dispatch = BuildDispatch("prismagamepadbuttonup", w3cIndex, skyrimCode);
            }

            const auto value = button->Value();
            const auto previous = g_buttonValues[w3cIndex].exchange(value, std::memory_order_relaxed);
            std::lock_guard lock(g_queueMutex);
            if (previous != value) {
                ultralight::GamepadButtonEvent event{};
                event.index = kGamepadIndex;
                event.button_index = w3cIndex;
                event.value = value;
                g_queue.emplace_back(event);
            }
            if (dispatch) {
                g_queue.emplace_back(std::move(*dispatch));
            }
        }

        void QueueThumbstick(const Platform::GameInput::ThumbstickEvent* thumbstick) {
            std::uint32_t xAxis = 0;
            std::uint32_t yAxis = 1;
            if (thumbstick->IsRight()) {
                xAxis = 2;
                yAxis = 3;
            } else if (!thumbstick->IsLeft()) {
                return;
            }

            ultralight::GamepadAxisEvent xEvent{};
            xEvent.index = kGamepadIndex;
            xEvent.axis_index = xAxis;
            xEvent.value = thumbstick->X();
            ultralight::GamepadAxisEvent yEvent{};
            yEvent.index = kGamepadIndex;
            yEvent.axis_index = yAxis;
            yEvent.value = -thumbstick->Y();
            std::lock_guard lock(g_queueMutex);
            g_queue.emplace_back(xEvent);
            g_queue.emplace_back(yEvent);
        }

        class GamepadSink final : public Platform::GameInput::Sink {
        public:
            static GamepadSink& Get() {
                static GamepadSink sink;
                return sink;
            }

            Platform::GameInput::NotifyControl ProcessEvent(
                Platform::GameInput::InputEvent* const* events,
                [[maybe_unused]] void* eventSource) override {
                if (!events || !*events || !InputHandler::IsAnyInputCaptureActive() || PrismaVR::IsVRActive()) {
                    return Platform::GameInput::NotifyControl::kContinue;
                }
                for (auto* event = *events; event; event = event->next) {
                    if (event->eventType == Platform::GameInput::EventType::kButton &&
                        event->device == Platform::GameInput::Device::kGamepad) {
                        QueueButton(reinterpret_cast<const Platform::GameInput::ButtonEvent*>(event));
                    } else if (event->eventType == Platform::GameInput::EventType::kThumbstick &&
                               event->device == Platform::GameInput::Device::kGamepad) {
                        QueueThumbstick(reinterpret_cast<const Platform::GameInput::ThumbstickEvent*>(event));
                    }
                }
                return Platform::GameInput::NotifyControl::kContinue;
            }
        };

        void QueueNeutralState() {
            std::lock_guard lock(g_queueMutex);
            for (std::uint32_t index = 0; index < kButtonCount; ++index) {
                ultralight::GamepadButtonEvent event{};
                event.index = kGamepadIndex;
                event.button_index = index;
                event.value = 0.0F;
                g_queue.emplace_back(event);
            }
            for (std::uint32_t index = 0; index < kAxisCount; ++index) {
                ultralight::GamepadAxisEvent event{};
                event.index = kGamepadIndex;
                event.axis_index = index;
                event.value = 0.0F;
                g_queue.emplace_back(event);
            }
        }

        void EnsureRegistered() {
            if (g_registered.exchange(true)) {
                return;
            }
            Core::renderer->SetGamepadDetails(kGamepadIndex, "Skyrim Controller (Standard Mapping)", kAxisCount,
                                              kButtonCount);
            ultralight::GamepadEvent event{};
            event.type = ultralight::GamepadEvent::kType_GamepadConnected;
            event.index = kGamepadIndex;
            Core::renderer->FireGamepadEvent(event);
        }
    }

    void ResetButtonValues() {
        for (auto& value : g_buttonValues) {
            value.store(0.0F, std::memory_order_relaxed);
        }
        if (InputHandler::GetFocusedViewId() == 0) {
            QueueNeutralState();
        }
    }

    void Initialize(SingleThreadExecutor* ultralightThreadExecutor) {
        ++g_session;
        g_ultralightThreadExecutor = ultralightThreadExecutor;
        g_registered = false;
        {
            std::lock_guard lock(g_queueMutex);
            g_queue.clear();
        }
        ResetButtonValues();
        if (!Platform::GameInput::AddSink(&GamepadSink::Get())) {
            logger::error("Failed to register the gamepad input sink");
        }
    }

    void ProcessEvents() {
        if (!g_ultralightThreadExecutor) {
            return;
        }
        std::vector<QueuedEvent> events;
        {
            std::lock_guard lock(g_queueMutex);
            if (g_queue.empty()) {
                return;
            }
            events.swap(g_queue);
        }
        const auto session = g_session.load();
        g_ultralightThreadExecutor->submit([events = std::move(events), session]() {
            if (!Core::renderer || session != g_session.load()) {
                return;
            }
            EnsureRegistered();
            for (const auto& event : events) {
                std::visit(
                    [](const auto& value) {
                        using T = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same_v<T, ultralight::GamepadButtonEvent>) {
                            Core::renderer->FireGamepadButtonEvent(value);
                        } else if constexpr (std::is_same_v<T, ultralight::GamepadAxisEvent>) {
                            Core::renderer->FireGamepadAxisEvent(value);
                        } else {
                            Communication::InvokeFromUltralightThread(value.viewId, value.script.c_str());
                        }
                    },
                    event);
            }
        });
    }

    void Shutdown() {
        ++g_session;
        Platform::GameInput::RemoveSink(&GamepadSink::Get());
        {
            std::lock_guard lock(g_queueMutex);
            g_queue.clear();
        }
        g_registered = false;
        g_ultralightThreadExecutor = nullptr;
    }
}