#pragma once

class SingleThreadExecutor;

namespace PrismaUI::GamepadInputHandler {
    // Registers the game input sink and stores the Ultralight executor.
    void Initialize(SingleThreadExecutor* ultralightThreadExecutor);

    // Drains queued Skyrim gamepad events and fires changes on the Ultralight renderer.
    void ProcessEvents();

    // Reset all "held" buttons so the next press registers as a fresh 0 -> 1 change.
    // Call on initialization and focus changes.
    // On focus loss it also pushes a neutral state to the Renderer so no button or axis stays latched
    // into the next focused view.
    void ResetButtonValues();

    // Removes the sink and clears queued state.
    void Shutdown();
}
