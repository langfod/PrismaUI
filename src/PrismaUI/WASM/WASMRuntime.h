#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

// Forward declarations for WAMR types (avoids including wasm_export.h in header)
struct WASMModuleCommon;
struct WASMModuleInstanceCommon;
struct WASMExecEnv;

namespace PrismaUI::WASM {

    // Opaque handle representing a live WASM instance for per-view tracking.
    // Stored in PrismaView::wasmInstances for cleanup on view destruction.
    struct WASMInstanceHandle {
        WASMModuleCommon* wasmModule = nullptr;
        WASMModuleInstanceCommon* moduleInst = nullptr;
        WASMExecEnv* execEnv = nullptr;
    };

    // Lazy initialization: creates the WAMR runtime on first use.
    // Must be called on the Ultralight thread. Returns true on success.
    bool EnsureRuntimeInitialized();

    // Increment the global live WASM object count (Module, Instance, Memory, etc.)
    void AddLiveObject();

    // Decrement the global live WASM object count and attempt lazy shutdown.
    void RemoveLiveObject();

    // Attempt to shut down the WAMR runtime if no live objects remain.
    void TryShutdownRuntime();

    // Force shutdown of the WAMR runtime. Called from Core::Shutdown().
    void ForceShutdownRuntime();

    // Destroy a WASM instance and release its WAMR resources.
    void DestroyInstance(WASMInstanceHandle& handle);

    // Check if the runtime is currently initialized.
    bool IsRuntimeInitialized();

}  // namespace PrismaUI::WASM
