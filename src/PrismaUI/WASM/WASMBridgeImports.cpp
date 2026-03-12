#include "WASMBridgeImports.h"

#include <JavaScriptCore/JavaScript.h>
#include <wasm_export.h>

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "WASMRuntime.h"


namespace PrismaUI::WASM {

    // =========================================================================
    // Signature string builder
    //
    // WAMR uses a compact signature format:
    //   '(' params ')' results
    //   'i' = i32, 'I' = i64, 'f' = f32, 'F' = f64
    //   Empty result = void
    // =========================================================================

    static char ValKindToSigChar(wasm_valkind_t kind) {
        switch (kind) {
            case WASM_I32:
                return 'i';
            case WASM_I64:
                return 'I';
            case WASM_F32:
                return 'f';
            case WASM_F64:
                return 'F';
            default:
                return 'i';  // Fallback
        }
    }

    static std::string BuildSignature(wasm_func_type_t funcType) {
        std::string sig = "(";

        uint32_t paramCount = wasm_func_type_get_param_count(funcType);
        for (uint32_t i = 0; i < paramCount; i++) {
            sig += ValKindToSigChar(wasm_func_type_get_param_valkind(funcType, i));
        }

        sig += ")";

        uint32_t resultCount = wasm_func_type_get_result_count(funcType);
        for (uint32_t i = 0; i < resultCount; i++) {
            sig += ValKindToSigChar(wasm_func_type_get_result_valkind(funcType, i));
        }

        return sig;
    }

    // =========================================================================
    // Host function trampoline
    //
    // This is the native function that WAMR calls when the WASM module invokes
    // an imported function. It uses the raw calling convention: arguments arrive
    // as a uint64* array, and the return value is written back to args[0].
    //
    // The TrampolineData* is retrieved via wasm_runtime_get_function_attachment().
    // =========================================================================

    static void HostFuncTrampoline(wasm_exec_env_t execEnv, uint64_t* args) {
        void* attachment = wasm_runtime_get_function_attachment(execEnv);
        auto* td = static_cast<ImportContext::TrampolineData*>(attachment);
        if (!td || !td->jsFunc) {
            args[0] = 0;
            wasm_module_inst_t mi = wasm_runtime_get_module_inst(execEnv);
            if (mi) wasm_runtime_set_exception(mi, "import function not found");
            return;
        }

        JSContextRef ctx = td->ctx;

        // Convert WASM args to JS values
        constexpr uint32_t kStackArgs = 16;
        JSValueRef stackArgs[kStackArgs];
        std::vector<JSValueRef> heapArgs;
        JSValueRef* jsArgs = stackArgs;
        if (td->paramCount > kStackArgs) {
            heapArgs.resize(td->paramCount);
            jsArgs = heapArgs.data();
        }
        uint64_t* argPtr = args;

        for (uint32_t i = 0; i < td->paramCount; i++) {
            wasm_valkind_t paramType = td->paramTypes[i];

            if (paramType == WASM_F64) {
                double val;
                memcpy(&val, argPtr, sizeof(double));
                jsArgs[i] = JSValueMakeNumber(ctx, val);
                argPtr++;
            } else if (paramType == WASM_F32) {
                float val;
                memcpy(&val, argPtr, sizeof(float));
                jsArgs[i] = JSValueMakeNumber(ctx, static_cast<double>(val));
                argPtr++;
            } else if (paramType == WASM_I64) {
                int64_t val;
                memcpy(&val, argPtr, sizeof(int64_t));
                jsArgs[i] = JSValueMakeNumber(ctx, static_cast<double>(val));
                argPtr++;
            } else {
                // i32
                int32_t val = static_cast<int32_t>(*argPtr);
                jsArgs[i] = JSValueMakeNumber(ctx, static_cast<double>(val));
                argPtr++;
            }
        }

        // Call the JS function
        JSValueRef exc = nullptr;
        JSValueRef result = JSObjectCallAsFunction(ctx, td->jsFunc, nullptr, td->paramCount,
                                                   td->paramCount > 0 ? jsArgs : nullptr, &exc);

        if (exc) {
            // Convert JS exception to WASM trap
            wasm_module_inst_t moduleInst = wasm_runtime_get_module_inst(execEnv);
            logger::error("[WASM] Import '{}': JS exception thrown", td->funcName);
            wasm_runtime_set_exception(moduleInst, "imported function threw an exception");
            return;
        }

        // Convert return value back to WASM
        if (td->resultCount > 0 && result) {
            wasm_valkind_t resultType = td->resultTypes[0];

            if (resultType == WASM_F64) {
                double val = JSValueToNumber(ctx, result, nullptr);
                memcpy(&args[0], &val, sizeof(double));
            } else if (resultType == WASM_F32) {
                float val = static_cast<float>(JSValueToNumber(ctx, result, nullptr));
                uint64_t slot = 0;
                memcpy(&slot, &val, sizeof(float));
                args[0] = slot;
            } else if (resultType == WASM_I64) {
                int64_t val = static_cast<int64_t>(JSValueToNumber(ctx, result, nullptr));
                memcpy(&args[0], &val, sizeof(int64_t));
            } else {
                // i32
                int32_t val = static_cast<int32_t>(JSValueToNumber(ctx, result, nullptr));
                args[0] = static_cast<uint64_t>(static_cast<uint32_t>(val));
            }
        }
    }

    // =========================================================================
    // ResolveImports
    // =========================================================================

    bool ResolveImports(JSContextRef ctx, wasm_module_t wasmModule, JSValueRef jsImports, ImportContext& outCtx,
                        JSValueRef* exception) {
        int32_t importCount = wasm_runtime_get_import_count(wasmModule);
        if (importCount <= 0) {
            return true;  // No imports needed
        }

        // If there are imports but no import object provided, that's an error
        if (!jsImports || JSValueIsUndefined(ctx, jsImports) || JSValueIsNull(ctx, jsImports)) {
            // Check if all imports are already linked (e.g., previously registered natives)
            bool allLinked = true;
            for (int32_t i = 0; i < importCount; i++) {
                wasm_import_t importInfo;
                wasm_runtime_get_import_type(wasmModule, i, &importInfo);
                if (!importInfo.linked) {
                    allLinked = false;
                    break;
                }
            }
            if (allLinked) return true;

            JSStringRef errStr =
                JSStringCreateWithUTF8CString("WebAssembly.Instance: module requires imports but none were provided");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return false;
        }

        JSObjectRef importsObj = JSValueToObject(ctx, jsImports, nullptr);
        if (!importsObj) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("WebAssembly.Instance: imports must be an object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return false;
        }

        // Group imports by module name. Use indices into the deque to avoid
        // any pointer/iterator stability concerns.
        std::unordered_map<std::string, size_t> moduleMap;

        for (int32_t i = 0; i < importCount; i++) {
            wasm_import_t importInfo;
            wasm_runtime_get_import_type(wasmModule, i, &importInfo);

            if (importInfo.linked) continue;  // Already resolved

            if (importInfo.kind != WASM_IMPORT_EXPORT_KIND_FUNC) {
                // Memory/Table/Global imports: WAMR handles memory imports
                // internally if the module defines default memory. Table and
                // Global imports are resolved by WAMR's default mechanisms
                // during instantiation.
                // TODO: Support explicit Memory/Table/Global import objects from JS
                // when use cases arise (e.g., sharing memory between modules).
                // Unlikely use case though, one wasm module is bad enough.
                logger::debug("[WASM] Skipping non-function import: {}.{} (kind={})",
                              importInfo.module_name ? importInfo.module_name : "",
                              importInfo.name ? importInfo.name : "", static_cast<int>(importInfo.kind));
                continue;
            }

            std::string modName = importInfo.module_name ? importInfo.module_name : "";
            std::string funcName = importInfo.name ? importInfo.name : "";

            // Look up imports[moduleName]
            JSStringRef modNameJS = JSStringCreateWithUTF8CString(modName.c_str());
            JSValueRef moduleVal = JSObjectGetProperty(ctx, importsObj, modNameJS, nullptr);
            JSStringRelease(modNameJS);

            if (!JSValueIsObject(ctx, moduleVal)) {
                std::string errMsg =
                    "WebAssembly.Instance: import module '" + modName + "' not found in imports object";
                JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
                JSValueRef errVal = JSValueMakeString(ctx, errStr);
                JSStringRelease(errStr);
                *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
                return false;
            }

            JSObjectRef moduleObj = JSValueToObject(ctx, moduleVal, nullptr);

            // Look up imports[moduleName][funcName]
            JSStringRef funcNameJS = JSStringCreateWithUTF8CString(funcName.c_str());
            JSValueRef funcVal = JSObjectGetProperty(ctx, moduleObj, funcNameJS, nullptr);
            JSStringRelease(funcNameJS);

            if (!JSValueIsObject(ctx, funcVal)) {
                std::string errMsg =
                    "WebAssembly.Instance: import '" + modName + "." + funcName + "' not found or not a function";
                JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
                JSValueRef errVal = JSValueMakeString(ctx, errStr);
                JSStringRelease(errStr);
                *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
                return false;
            }

            JSObjectRef funcObj = JSValueToObject(ctx, funcVal, nullptr);
            if (!funcObj || !JSObjectIsFunction(ctx, funcObj)) {
                std::string errMsg =
                    "WebAssembly.Instance: import '" + modName + "." + funcName + "' is not a function";
                JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
                JSValueRef errVal = JSValueMakeString(ctx, errStr);
                JSStringRelease(errStr);
                *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
                return false;
            }

            // Protect the JS function from GC during WASM execution
            JSValueProtect(ctx, funcObj);

            // Build type info for the trampoline
            uint32_t paramCount = wasm_func_type_get_param_count(importInfo.u.func_type);
            uint32_t resultCount = wasm_func_type_get_result_count(importInfo.u.func_type);

            std::vector<wasm_valkind_t> paramTypes(paramCount);
            for (uint32_t p = 0; p < paramCount; p++) {
                paramTypes[p] = wasm_func_type_get_param_valkind(importInfo.u.func_type, p);
            }

            std::vector<wasm_valkind_t> resultTypes(resultCount);
            for (uint32_t r = 0; r < resultCount; r++) {
                resultTypes[r] = wasm_func_type_get_result_valkind(importInfo.u.func_type, r);
            }

            auto* td = new ImportContext::TrampolineData{
                ctx, funcObj, paramCount, resultCount, std::move(paramTypes), std::move(resultTypes), funcName};
            outCtx.trampolines.push_back(td);

            // Build WAMR signature string
            std::string sig = BuildSignature(importInfo.u.func_type);

            // Find or create the module symbols group
            auto mapIt = moduleMap.find(modName);
            if (mapIt == moduleMap.end()) {
                outCtx.modules.push_back({modName, {}, {}});
                moduleMap[modName] = outCtx.modules.size() - 1;
                mapIt = moduleMap.find(modName);
            }

            auto& modSyms = outCtx.modules[mapIt->second];

            // Store signature and function name into stable deque storage
            modSyms.signatureStorage.push_back(std::move(sig));
            modSyms.signatureStorage.push_back(std::move(funcName));

            // Build the NativeSymbol entry — pointers into deque elements are stable
            NativeSymbol sym;
            sym.symbol = modSyms.signatureStorage[modSyms.signatureStorage.size() - 1].c_str();
            sym.func_ptr = reinterpret_cast<void*>(HostFuncTrampoline);
            sym.signature = modSyms.signatureStorage[modSyms.signatureStorage.size() - 2].c_str();
            sym.attachment = td;
            modSyms.symbols.push_back(sym);
        }

        // Register all module symbol groups with WAMR
        for (auto& modSyms : outCtx.modules) {
            if (modSyms.symbols.empty()) continue;

            bool ok = wasm_runtime_register_natives_raw(modSyms.moduleName.c_str(), modSyms.symbols.data(),
                                                        static_cast<uint32_t>(modSyms.symbols.size()));

            if (!ok) {
                std::string errMsg =
                    "WebAssembly.Instance: failed to register native imports for module '" + modSyms.moduleName + "'";
                logger::error("[WASM] {}", errMsg);

                CleanupImports(outCtx);

                JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
                JSValueRef errVal = JSValueMakeString(ctx, errStr);
                JSStringRelease(errStr);
                *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
                return false;
            }

            logger::info("[WASM] Registered {} native imports for module '{}'", modSyms.symbols.size(),
                         modSyms.moduleName);
            for (size_t s = 0; s < modSyms.symbols.size(); s++) {
                logger::info("[WASM]   import[{}]: '{}' sig='{}'", s,
                             modSyms.symbols[s].symbol ? modSyms.symbols[s].symbol : "?",
                             modSyms.symbols[s].signature ? modSyms.symbols[s].signature : "?");
            }
        }

        return true;
    }

    // =========================================================================
    // CleanupImports
    // =========================================================================

    void CleanupImports(ImportContext& ctx) {
        UnregisterImportNatives(ctx);
        CleanupImportTrampolines(ctx);
    }

    void UnregisterImportNatives(ImportContext& ctx) {
        // Unregister native symbols from WAMR's global table so they don't
        // interfere with future instantiations of other modules.
        for (auto& modSyms : ctx.modules) {
            if (!modSyms.symbols.empty()) {
                wasm_runtime_unregister_natives(modSyms.moduleName.c_str(), modSyms.symbols.data());
            }
        }
        ctx.modules.clear();
    }

    void CleanupImportTrampolines(ImportContext& ctx) {
        // Unprotect JS function references and free trampoline data
        for (auto* td : ctx.trampolines) {
            if (td->jsFunc && td->ctx) {
                JSValueUnprotect(td->ctx, td->jsFunc);
            }
            delete td;
        }
        ctx.trampolines.clear();
    }

}  // namespace PrismaUI::WASM
