#pragma once

#include <JavaScriptCore/JavaScript.h>
#include <wasm_export.h>

#include <cstdint>

namespace PrismaUI::WASM {

    // Inject WebAssembly API bindings into a JS context.
    // Creates the WebAssembly global object with validate, compile, instantiate,
    // Module, Instance constructors, and Promise helpers.
    // Called from OnWindowObjectReady alongside WebGL injection.
    void InjectWASMBindings(JSContextRef jsCtx, uint64_t viewId);

    // SEH-guarded wrapper around wasm_runtime_call_wasm.
    // On Windows, catches hardware exceptions (access violations, etc.)
    // and logs a full stack trace before returning false.
    bool SEHCallWasm(wasm_exec_env_t execEnv, wasm_function_inst_t func,
                     uint32_t argc, uint32_t* argv);

}  // namespace PrismaUI::WASM
