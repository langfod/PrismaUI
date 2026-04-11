#pragma once

#include <JavaScriptCore/JavaScript.h>
#include <wasm_export.h>

#include <cstdint>

namespace PrismaUI::WASM {

    void InjectWASMBindings(JSContextRef jsCtx, uint64_t viewId);

    bool SEHCallWasm(wasm_exec_env_t execEnv, wasm_function_inst_t func, uint32_t argc, uint32_t* argv);

}  // namespace PrismaUI::WASM
