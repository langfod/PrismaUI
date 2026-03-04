#pragma once

#include <JavaScriptCore/JavaScript.h>
#include <wasm_export.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace PrismaUI::WASM {

    // =========================================================================
    // Import resolution: walk a JS import object and register native symbols
    // with WAMR so that wasm_runtime_instantiate() can resolve them.
    //
    // Usage:
    //   ImportContext imports;
    //   if (!ResolveImports(ctx, wasmModule, jsImportObj, imports, exception))
    //       return error;
    //   wasm_module_inst_t inst = wasm_runtime_instantiate(wasmModule, ...);
    //   // After instantiation (or on failure), always clean up:
    //   CleanupImports(imports);
    // =========================================================================

    // Holds the state for one batch of import registrations so we can
    // unregister them after instantiation.
    struct ImportContext {
        // Per-module native symbol arrays. Each entry corresponds to one WASM
        // import module name (e.g. "env", "wasi_snapshot_preview1").
        struct ModuleSymbols {
            std::string moduleName;
            std::vector<NativeSymbol> symbols;
            // Storage for dynamically-built signature and name strings.
            // Uses std::deque so that push_back never invalidates existing
            // elements — NativeSymbol.symbol and .signature are raw pointers
            // into these strings and must remain valid until cleanup.
            std::deque<std::string> signatureStorage;
        };
        std::deque<ModuleSymbols> modules;

        // Per-import trampoline context. Stored here for lifetime management.
        struct TrampolineData {
            JSContextRef ctx;
            JSObjectRef jsFunc;       // Protected JS function reference
            uint32_t paramCount;
            uint32_t resultCount;
            std::vector<wasm_valkind_t> paramTypes;
            std::vector<wasm_valkind_t> resultTypes;
            std::string funcName;     // For diagnostics (e.g. "_emscripten_memcpy_big")
        };
        std::vector<TrampolineData*> trampolines;
    };

    // Walk the JS imports object, match against the module's import declarations,
    // build NativeSymbol arrays, and register them with WAMR.
    // Returns true on success. On failure, sets *exception and returns false.
    // The caller must call CleanupImports() regardless of success/failure.
    bool ResolveImports(JSContextRef ctx, wasm_module_t wasmModule,
                        JSValueRef jsImports, ImportContext& outCtx,
                        JSValueRef* exception);

    // Unregister all native symbols that were registered by ResolveImports,
    // and free associated trampoline data.
    void CleanupImports(ImportContext& ctx);

    // Unregister native symbols from WAMR's global table only.
    // Call this after wasm_runtime_instantiate() so the global registrations
    // don't interfere with future instantiations. Does NOT free trampoline data
    // or unprotect JS refs — those must remain alive while the instance exists.
    void UnregisterImportNatives(ImportContext& ctx);

    // Free trampoline data and unprotect JS function references.
    // Call this when the WASM instance is being destroyed (e.g. in the finalizer).
    void CleanupImportTrampolines(ImportContext& ctx);

}  // namespace PrismaUI::WASM
