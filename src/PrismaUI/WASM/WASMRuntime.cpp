#include "WASMRuntime.h"

#include <wasm_export.h>

#include <cstring>

namespace PrismaUI::WASM {

    static std::atomic<bool> g_runtimeInitialized = false;
    static std::atomic<int> g_liveObjectCount = 0;

    bool EnsureRuntimeInitialized() {
        if (g_runtimeInitialized.load(std::memory_order_acquire)) return true;

        RuntimeInitArgs args;
        memset(&args, 0, sizeof(args));
        args.mem_alloc_type = Alloc_With_System_Allocator;

        if (!wasm_runtime_full_init(&args)) {
            logger::error("[WASM] Failed to initialize WAMR runtime");
            return false;
        }

        g_runtimeInitialized.store(true, std::memory_order_release);
        logger::info("[WASM] WAMR runtime initialized (on first use)");
        return true;
    }

    void AddLiveObject() {
        g_liveObjectCount.fetch_add(1, std::memory_order_relaxed);
    }

    void RemoveLiveObject() {
        int prev = g_liveObjectCount.fetch_sub(1, std::memory_order_acq_rel);
        if (prev <= 1) {
            TryShutdownRuntime();
        }
    }

    void TryShutdownRuntime() {
        if (g_liveObjectCount.load(std::memory_order_acquire) <= 0 &&
            g_runtimeInitialized.load(std::memory_order_acquire)) {
            wasm_runtime_destroy();
            g_runtimeInitialized.store(false, std::memory_order_release);
            g_liveObjectCount.store(0, std::memory_order_relaxed);
            logger::info("[WASM] WAMR runtime shut down (no live objects)");
        }
    }

    void ForceShutdownRuntime() {
        if (g_runtimeInitialized.load(std::memory_order_acquire)) {
            wasm_runtime_destroy();
            g_runtimeInitialized.store(false, std::memory_order_release);
            g_liveObjectCount.store(0, std::memory_order_relaxed);
            logger::info("[WASM] WAMR runtime force shut down");
        }
    }

    void DestroyInstance(WASMInstanceHandle& handle) {
        if (handle.execEnv) {
            wasm_runtime_destroy_exec_env(handle.execEnv);
            handle.execEnv = nullptr;
        }
        if (handle.moduleInst) {
            wasm_runtime_deinstantiate(handle.moduleInst);
            handle.moduleInst = nullptr;
        }
        if (handle.wasmModule) {
            wasm_runtime_unload(handle.wasmModule);
            handle.wasmModule = nullptr;
        }
    }

    bool IsRuntimeInitialized() {
        return g_runtimeInitialized.load(std::memory_order_acquire);
    }

}  // namespace PrismaUI::WASM
