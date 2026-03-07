#include "WASMBridge.h"
#include "WASMBridgeImports.h"
#include "WASMBridgeObjects.h"
#include "WASMRuntime.h"

#include "PrismaUI/Core.h"

#include <JavaScriptCore/JavaScript.h>
#include <wasm_export.h>

#include <cstdlib>
#include <cstring>
#include <shared_mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp.lib")
#endif

namespace PrismaUI::WASM {

    // =========================================================================
    // SEH-guarded WASM execution wrapper (Windows only)
    //
    // __try/__except cannot coexist with C++ objects that have destructors
    // in the same function scope, so we isolate the call in a thin wrapper.
    // =========================================================================

#ifdef _WIN32
    static LONG WINAPI LogSEHException(EXCEPTION_POINTERS* ep) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        void* addr = ep->ExceptionRecord->ExceptionAddress;

        logger::error("[WASM] SEH exception during WASM execution:");
        logger::error("[WASM]   Code:    0x{:08X}", code);
        logger::error("[WASM]   Address: {}", addr);

        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
            ULONG_PTR accessType = ep->ExceptionRecord->ExceptionInformation[0];
            const char* op = accessType == 0 ? "read" : accessType == 1 ? "write" : "execute (DEP)";
            void* target = reinterpret_cast<void*>(ep->ExceptionRecord->ExceptionInformation[1]);
            logger::error("[WASM]   Access violation: {} at address {}", op, target);
        }

        // Dump CPU registers — critical for identifying which pointer is bad
        CONTEXT* context = ep->ContextRecord;
        logger::error("[WASM]   Registers:");
        logger::error("[WASM]     RAX=0x{:016x}  RBX=0x{:016x}", context->Rax, context->Rbx);
        logger::error("[WASM]     RCX=0x{:016x}  RDX=0x{:016x}", context->Rcx, context->Rdx);
        logger::error("[WASM]     RSI=0x{:016x}  RDI=0x{:016x}", context->Rsi, context->Rdi);
        logger::error("[WASM]     R8 =0x{:016x}  R9 =0x{:016x}", context->R8,  context->R9);
        logger::error("[WASM]     R10=0x{:016x}  R11=0x{:016x}", context->R10, context->R11);
        logger::error("[WASM]     R12=0x{:016x}  R13=0x{:016x}", context->R12, context->R13);
        logger::error("[WASM]     R14=0x{:016x}  R15=0x{:016x}", context->R14, context->R15);
        logger::error("[WASM]     RIP=0x{:016x}  RSP=0x{:016x}  RBP=0x{:016x}",
                      context->Rip, context->Rsp, context->Rbp);

        // Walk the stack
        HANDLE process = GetCurrentProcess();
        HANDLE thread = GetCurrentThread();
        SymInitialize(process, NULL, TRUE);

        // Also resolve the crashing instruction's symbol with source line
        {
            char symbolBuf[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuf);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;
            DWORD64 displacement = 0;
            if (SymFromAddr(process, context->Rip, &displacement, symbol)) {
                logger::error("[WASM]   Crash in: {}+0x{:x}", symbol->Name, displacement);
            }
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisp = 0;
            if (SymGetLineFromAddr64(process, context->Rip, &lineDisp, &line)) {
                logger::error("[WASM]   Source: {}:{}", line.FileName, line.LineNumber);
            }
        }

        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context->Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context->Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context->Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        logger::error("[WASM]   Stack trace:");
        for (int i = 0; i < 32; i++) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread,
                             &frame, context, NULL,
                             SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
                break;
            }
            if (frame.AddrPC.Offset == 0) break;

            char symbolBuf[sizeof(SYMBOL_INFO) + 256];
            SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuf);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 255;

            DWORD64 displacement = 0;
            if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
                logger::error("[WASM]     [{}] {}+0x{:x}",
                              i, symbol->Name, displacement);
            } else {
                logger::error("[WASM]     [{}] 0x{:016x}", i, frame.AddrPC.Offset);
            }
        }

        SymCleanup(process);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    // Thin SEH wrapper — no C++ destructors allowed in this function.
    // After catching an exception, set WAMR's internal exception string so the
    // module instance is in a consistent error state.
    bool SEHCallWasm(wasm_exec_env_t execEnv, wasm_function_inst_t func,
                     uint32_t argc, uint32_t* argv) {
        // Clear any leftover exception from a previous failed call so the
        // interpreter doesn't refuse to run.
        wasm_module_inst_t mi = wasm_runtime_get_module_inst(execEnv);
        if (mi) {
            wasm_runtime_clear_exception(mi);
        }

        __try {
            return wasm_runtime_call_wasm(execEnv, func, argc, argv);
        }
        __except (LogSEHException(GetExceptionInformation())) {
            // The exception handler already logged everything.
            logger::error("[WASM] SEH handler: returning false to caller");
            // Try to set an exception on the module and dump the WASM call
            // stack.  Guard this section — exec_env or module_inst may be
            // corrupted after the crash.
            __try {
                wasm_module_inst_t mi2 = wasm_runtime_get_module_inst(execEnv);
                if (mi2) {
                    wasm_runtime_set_exception(mi2,
                        "access violation caught by SEH guard");
                }
                // Dump WASM-level call stack (requires WAMR_BUILD_DUMP_CALL_STACK=1).
                // Use malloc/free instead of std::vector — SEH blocks cannot
                // contain C++ objects with destructors.
                uint32_t csSize = wasm_runtime_get_call_stack_buf_size(execEnv);
                if (csSize > 0 && csSize < 64 * 1024) {
                    char* csBuf = static_cast<char*>(malloc(csSize));
                    if (csBuf) {
                        uint32_t written = wasm_runtime_dump_call_stack_to_buf(
                            execEnv, csBuf, csSize);
                        if (written > 0) {
                            logger::error("[WASM] WASM call stack at crash:\n{}",
                                          csBuf);
                        }
                        free(csBuf);
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                logger::error("[WASM] Could not dump WASM call stack (exec_env corrupted)");
            }
            return false;
        }
    }
#else
    // Non-Windows: no SEH, call directly
    bool SEHCallWasm(wasm_exec_env_t execEnv, wasm_function_inst_t func,
                     uint32_t argc, uint32_t* argv) {
        return wasm_runtime_call_wasm(execEnv, func, argc, argv);
    }
#endif

    // =========================================================================
    // Private data structs stored as JSObject private data
    // =========================================================================

    struct WASMModuleData {
        wasm_module_t wasmModule = nullptr;
        std::vector<uint8_t> bytecode;  // Keep bytecode alive while module exists
    };

    struct WASMInstanceData {
        wasm_module_inst_t moduleInst = nullptr;
        wasm_exec_env_t execEnv = nullptr;
        wasm_module_t wasmModule = nullptr;  // Non-owning ref for export queries
        uint64_t viewId = 0;
        ImportContext importCtx;  // Trampoline data kept alive for imported function calls
        bool poisoned = false;   // Set after an SEH crash — shared across all exports
        // prevent the Module JSObject from being GC'd while this instance exists.
        // The WAMR module inst holds raw pointers into module data (function bytecode,
        // type info); if the Module finalizer runs wasm_runtime_unload() while the
        // instance is still alive, those pointers become dangling → use-after-free.
        JSContextRef moduleCtx = nullptr;
        JSObjectRef  moduleRef = nullptr;   // JSValueProtect'd (null until set)
    };

    struct ExportFuncContext {
        wasm_exec_env_t execEnv;
        wasm_module_inst_t moduleInst;  // Needed for type queries
        wasm_function_inst_t wasmFunc;
        uint32_t paramCount;
        uint32_t resultCount;
        std::vector<wasm_valkind_t> paramTypes;
        std::vector<wasm_valkind_t> resultTypes;
        // prevent the WASM instance JSObject from being GC'd while this
        // export function exists — the raw pointers above reference
        // resources owned by the instance.
        JSContextRef ctx;
        JSObjectRef instanceRef;  // JSValueProtect'd
        bool* instancePoisonFlag = nullptr;  // Points to WASMInstanceData::poisoned
        std::string funcName;     // For diagnostics
    };

    // =========================================================================
    // JSC Class definitions
    // =========================================================================

    static JSClassRef g_WASMModuleClass = nullptr;
    static JSClassRef g_WASMInstanceClass = nullptr;
    static JSClassRef g_ExportFuncClass = nullptr;

    // --- Module class ---

    static void WASMModuleFinalize(JSObjectRef obj) {
        auto* data = static_cast<WASMModuleData*>(JSObjectGetPrivate(obj));
        if (data) {
            if (data->wasmModule) {
                wasm_runtime_unload(data->wasmModule);
            }
            delete data;
            RemoveLiveObject();
        }
    }

    static JSClassRef GetWASMModuleClass() {
        if (!g_WASMModuleClass) {
            JSClassDefinition def{};
            def.className = "WebAssembly.Module";
            def.finalize = WASMModuleFinalize;
            g_WASMModuleClass = JSClassCreate(&def);
        }
        return g_WASMModuleClass;
    }

    // --- Export function class ---

    static void ExportFuncFinalize(JSObjectRef obj) {
        auto* efc = static_cast<ExportFuncContext*>(JSObjectGetPrivate(obj));
        if (efc) {
            if (efc->instanceRef && efc->ctx) {
                JSValueUnprotect(efc->ctx, efc->instanceRef);
            }
            delete efc;
        }
    }

    static JSClassRef GetExportFuncClass() {
        if (!g_ExportFuncClass) {
            JSClassDefinition def{};
            def.className = "WebAssembly.ExportedFunction";
            def.finalize = ExportFuncFinalize;
            def.callAsFunction = [](JSContextRef ctx, JSObjectRef function,
                                    JSObjectRef /*thisObject*/, size_t argc,
                                    const JSValueRef argv[],
                                    JSValueRef* exception) -> JSValueRef {
                auto* efc = static_cast<ExportFuncContext*>(JSObjectGetPrivate(function));
                if (!efc || !efc->execEnv || !efc->wasmFunc) {
                    return JSValueMakeUndefined(ctx);
                }

                // After an SEH crash the module instance is in an undefined
                // state.  Refuse further calls to prevent a cascade of
                // repeated access violations (e.g. requestAnimationFrame).
                if (efc->instancePoisonFlag && *efc->instancePoisonFlag) {
                    JSStringRef errStr = JSStringCreateWithUTF8CString(
                        "WASM instance terminated due to previous fatal error");
                    JSValueRef errVal = JSValueMakeString(ctx, errStr);
                    JSStringRelease(errStr);
                    *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
                    return JSValueMakeUndefined(ctx);
                }

                // Build argv for WAMR: each WASM param is a uint32_t slot
                // (i64/f64 take 2 slots in the uint32_t array)
                // Allocate generous space: 2 slots per param + 2 for result
                uint32_t maxSlots = (efc->paramCount + efc->resultCount) * 2;
                if (maxSlots < 2) maxSlots = 2;

                constexpr uint32_t kStackSlots = 32;
                uint32_t stackBuf[kStackSlots] = {};
                std::vector<uint32_t> heapBuf;
                uint32_t* wasmArgv = stackBuf;
                if (maxSlots > kStackSlots) {
                    heapBuf.resize(maxSlots, 0);
                    wasmArgv = heapBuf.data();
                }

                // Convert JS args to WASM args based on type info
                uint32_t slotIdx = 0;
                for (uint32_t i = 0; i < efc->paramCount && i < static_cast<uint32_t>(argc); i++) {
                    wasm_valkind_t paramType = (i < efc->paramTypes.size()) ?
                        efc->paramTypes[i] : WASM_I32;

                    if (paramType == WASM_F64) {
                        double val = JSValueToNumber(ctx, argv[i], nullptr);
                        memcpy(&wasmArgv[slotIdx], &val, sizeof(double));
                        slotIdx += 2;  // f64 takes 2 uint32_t slots
                    } else if (paramType == WASM_F32) {
                        float val = static_cast<float>(JSValueToNumber(ctx, argv[i], nullptr));
                        memcpy(&wasmArgv[slotIdx], &val, sizeof(float));
                        slotIdx += 1;
                    } else if (paramType == WASM_I64) {
                        int64_t val = static_cast<int64_t>(JSValueToNumber(ctx, argv[i], nullptr));
                        memcpy(&wasmArgv[slotIdx], &val, sizeof(int64_t));
                        slotIdx += 2;  // i64 takes 2 uint32_t slots
                    } else {
                        // i32 or other
                        wasmArgv[slotIdx] = static_cast<uint32_t>(
                            static_cast<int32_t>(JSValueToNumber(ctx, argv[i], nullptr)));
                        slotIdx += 1;
                    }
                }

                if (!SEHCallWasm(efc->execEnv, efc->wasmFunc,
                                 slotIdx, wasmArgv)) {
                    // Guard: module_inst may be invalid after SEH exception
                    const char* exceptionMsg = nullptr;
                    wasm_module_inst_t mi = wasm_runtime_get_module_inst(efc->execEnv);
                    if (mi) {
                        exceptionMsg = wasm_runtime_get_exception(mi);
                    }
                    std::string errStr = exceptionMsg ? exceptionMsg : "WASM runtime error (possible SEH crash)";
                    logger::error("[WASM] Export '{}' failed: {}",
                                  efc->funcName, errStr);

                    // If the error is from an SEH catch (access violation),
                    // poison the ENTIRE instance so no export can be called.
                    if (errStr.find("access violation") != std::string::npos
                        || errStr.find("SEH") != std::string::npos
                        || !exceptionMsg) {
                        if (efc->instancePoisonFlag)
                            *efc->instancePoisonFlag = true;
                        logger::error("[WASM] Instance poisoned — further calls will be rejected");
                    }

                    JSStringRef errJSStr = JSStringCreateWithUTF8CString(errStr.c_str());
                    JSValueRef errVal = JSValueMakeString(ctx, errJSStr);
                    JSStringRelease(errJSStr);
                    *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
                    return JSValueMakeUndefined(ctx);
                }

                // Return value
                if (efc->resultCount == 0) {
                    return JSValueMakeUndefined(ctx);
                }

                wasm_valkind_t resultType = efc->resultTypes.empty() ?
                    WASM_I32 : efc->resultTypes[0];

                if (resultType == WASM_F64) {
                    double result;
                    memcpy(&result, wasmArgv, sizeof(double));
                    return JSValueMakeNumber(ctx, result);
                } else if (resultType == WASM_F32) {
                    float result;
                    memcpy(&result, wasmArgv, sizeof(float));
                    return JSValueMakeNumber(ctx, static_cast<double>(result));
                } else if (resultType == WASM_I64) {
                    int64_t result;
                    memcpy(&result, wasmArgv, sizeof(int64_t));
                    return JSValueMakeNumber(ctx, static_cast<double>(result));
                } else {
                    // i32
                    return JSValueMakeNumber(ctx, static_cast<double>(
                        static_cast<int32_t>(wasmArgv[0])));
                }
            };
            g_ExportFuncClass = JSClassCreate(&def);
        }
        return g_ExportFuncClass;
    }

    // --- Instance class ---

    static void WASMInstanceFinalize(JSObjectRef obj) {
        auto* data = static_cast<WASMInstanceData*>(JSObjectGetPrivate(obj));
        if (data) {
            // Remove from view's tracking list
            {
                std::shared_lock lock(Core::viewsMutex);
                auto it = Core::views.find(data->viewId);
                if (it != Core::views.end() && it->second->wasmInstances) {
                    auto& instances = *it->second->wasmInstances;
                    for (auto instIt = instances.begin(); instIt != instances.end(); ++instIt) {
                        if (instIt->moduleInst == data->moduleInst) {
                            instances.erase(instIt);
                            break;
                        }
                    }
                }
            }

            // Unregister import natives from WAMR's global lookup table, then
            // clean up import trampolines (unprotect JS refs, free trampoline data).
            // Both must be deferred to here because WAMR reads both the
            // NativeSymbol data (signature pointers) and trampoline attachments
            // at call time throughout the instance's lifetime.
            UnregisterImportNatives(data->importCtx);
            CleanupImportTrampolines(data->importCtx);

            if (data->execEnv) {
                wasm_runtime_destroy_exec_env(data->execEnv);
            }
            if (data->moduleInst) {
                wasm_runtime_deinstantiate(data->moduleInst);
            }

            // Release the GC protection on the Module JSObject. This must happen
            // AFTER wasm_runtime_deinstantiate() because deinstantiation still
            // accesses module data. Once done, the module can be GC'd normally.
            if (data->moduleRef) {
                JSValueUnprotect(data->moduleCtx, data->moduleRef);
            }

            delete data;
            RemoveLiveObject();
        }
    }

    static JSClassRef GetWASMInstanceClass() {
        if (!g_WASMInstanceClass) {
            JSClassDefinition def{};
            def.className = "WebAssembly.Instance";
            def.finalize = WASMInstanceFinalize;
            g_WASMInstanceClass = JSClassCreate(&def);
        }
        return g_WASMInstanceClass;
    }

    // =========================================================================
    // Byte extraction helpers
    // =========================================================================

    static std::pair<const uint8_t*, size_t> ExtractBytes(JSContextRef ctx, JSValueRef arg) {
        JSObjectRef obj = JSValueToObject(ctx, arg, nullptr);
        if (!obj) return {nullptr, 0};

        // Try ArrayBuffer first
        JSValueRef exc = nullptr;
        void* ptr = JSObjectGetArrayBufferBytesPtr(ctx, obj, &exc);
        if (ptr && !exc) {
            size_t len = JSObjectGetArrayBufferByteLength(ctx, obj, nullptr);
            return {static_cast<const uint8_t*>(ptr), len};
        }

        // Try TypedArray
        exc = nullptr;
        ptr = JSObjectGetTypedArrayBytesPtr(ctx, obj, &exc);
        if (ptr && !exc) {
            size_t len = JSObjectGetTypedArrayByteLength(ctx, obj, nullptr);
            size_t offset = JSObjectGetTypedArrayByteOffset(ctx, obj, nullptr);
            return {static_cast<const uint8_t*>(ptr) + offset, len};
        }

        return {nullptr, 0};
    }

    // =========================================================================
    // Promise helpers (Approach A: pre-installed JS functions)
    // =========================================================================

    static JSValueRef ResolvePromise(JSContextRef ctx, JSValueRef value) {
        JSObjectRef global = JSContextGetGlobalObject(ctx);
        JSStringRef name = JSStringCreateWithUTF8CString("__prismaWASMResolve");
        JSValueRef resolverVal = JSObjectGetProperty(ctx, global, name, nullptr);
        JSStringRelease(name);

        if (!JSValueIsObject(ctx, resolverVal)) return value;
        JSObjectRef resolver = JSValueToObject(ctx, resolverVal, nullptr);
        if (!resolver) return value;

        return JSObjectCallAsFunction(ctx, resolver, nullptr, 1, &value, nullptr);
    }

    static JSValueRef RejectPromise(JSContextRef ctx, const std::string& errorMsg,
                                     const char* errorType = "Error") {
        // Create error object of the appropriate type
        JSStringRef msgStr = JSStringCreateWithUTF8CString(errorMsg.c_str());
        JSValueRef msgVal = JSValueMakeString(ctx, msgStr);
        JSStringRelease(msgStr);

        // Try to use the specific error constructor (e.g., WebAssembly.CompileError)
        JSObjectRef global = JSContextGetGlobalObject(ctx);
        JSStringRef waName = JSStringCreateWithUTF8CString("WebAssembly");
        JSValueRef waVal = JSObjectGetProperty(ctx, global, waName, nullptr);
        JSStringRelease(waName);

        JSValueRef errorObj = nullptr;
        if (JSValueIsObject(ctx, waVal)) {
            JSObjectRef waObj = JSValueToObject(ctx, waVal, nullptr);
            JSStringRef errTypeName = JSStringCreateWithUTF8CString(errorType);
            JSValueRef errTypeVal = JSObjectGetProperty(ctx, waObj, errTypeName, nullptr);
            JSStringRelease(errTypeName);

            if (JSValueIsObject(ctx, errTypeVal)) {
                JSObjectRef errTypeCtor = JSValueToObject(ctx, errTypeVal, nullptr);
                if (errTypeCtor && JSObjectIsConstructor(ctx, errTypeCtor)) {
                    errorObj = JSObjectCallAsConstructor(ctx, errTypeCtor, 1, &msgVal, nullptr);
                }
            }
        }

        if (!errorObj) {
            errorObj = JSObjectMakeError(ctx, 1, &msgVal, nullptr);
        }

        JSStringRef rejectName = JSStringCreateWithUTF8CString("__prismaWASMReject");
        JSValueRef rejecterVal = JSObjectGetProperty(ctx, global, rejectName, nullptr);
        JSStringRelease(rejectName);

        if (JSValueIsObject(ctx, rejecterVal)) {
            JSObjectRef rejecter = JSValueToObject(ctx, rejecterVal, nullptr);
            if (rejecter) {
                return JSObjectCallAsFunction(ctx, rejecter, nullptr, 1, &errorObj, nullptr);
            }
        }

        return JSValueMakeUndefined(ctx);
    }

    // Helper to convert an exception to a rejected promise and clear the exception
    static JSValueRef ExceptionToRejectedPromise(JSContextRef ctx, JSValueRef* exception) {
        JSValueRef exc = *exception;
        *exception = nullptr;

        JSObjectRef global = JSContextGetGlobalObject(ctx);
        JSStringRef rejectName = JSStringCreateWithUTF8CString("__prismaWASMReject");
        JSValueRef rejecterVal = JSObjectGetProperty(ctx, global, rejectName, nullptr);
        JSStringRelease(rejectName);
        if (JSValueIsObject(ctx, rejecterVal)) {
            JSObjectRef rejecter = JSValueToObject(ctx, rejecterVal, nullptr);
            if (rejecter) {
                return JSObjectCallAsFunction(ctx, rejecter, nullptr, 1, &exc, nullptr);
            }
        }
        return JSValueMakeUndefined(ctx);
    }

    // =========================================================================
    // Build exports object from a WASM instance
    // =========================================================================

    // Ensure a callable JSC object inherits from Function.prototype so that
    // .apply(), .call(), and .bind() work — required by Emscripten glue code.
    static void SetFunctionPrototype(JSContextRef ctx, JSObjectRef obj) {
        JSObjectRef global = JSContextGetGlobalObject(ctx);
        JSStringRef funcName = JSStringCreateWithUTF8CString("Function");
        JSValueRef funcVal = JSObjectGetProperty(ctx, global, funcName, nullptr);
        JSStringRelease(funcName);
        if (JSValueIsObject(ctx, funcVal)) {
            JSObjectRef funcCtor = JSValueToObject(ctx, funcVal, nullptr);
            JSStringRef protoName = JSStringCreateWithUTF8CString("prototype");
            JSValueRef protoVal = JSObjectGetProperty(ctx, funcCtor, protoName, nullptr);
            JSStringRelease(protoName);
            JSObjectSetPrototype(ctx, obj, protoVal);
        }
    }

    static JSObjectRef BuildExportsObject(JSContextRef ctx, WASMInstanceData* instData,
                                           JSObjectRef instanceObj) {
        JSObjectRef exportsObj = JSObjectMake(ctx, nullptr, nullptr);

        int32_t exportCount = wasm_runtime_get_export_count(instData->wasmModule);
        for (int32_t i = 0; i < exportCount; i++) {
            wasm_export_t exportInfo;
            wasm_runtime_get_export_type(instData->wasmModule, i, &exportInfo);

            if (!exportInfo.name) continue;

            if (exportInfo.kind == WASM_IMPORT_EXPORT_KIND_FUNC) {
                wasm_function_inst_t func = wasm_runtime_lookup_function(
                    instData->moduleInst, exportInfo.name);
                if (!func) continue;

                uint32_t paramCount = wasm_func_get_param_count(func, instData->moduleInst);
                uint32_t resultCount = wasm_func_get_result_count(func, instData->moduleInst);

                // Pre-fetch param and result types
                std::vector<wasm_valkind_t> paramTypes(paramCount);
                std::vector<wasm_valkind_t> resultTypes(resultCount);
                if (paramCount > 0) {
                    wasm_func_get_param_types(func, instData->moduleInst, paramTypes.data());
                }
                if (resultCount > 0) {
                    wasm_func_get_result_types(func, instData->moduleInst, resultTypes.data());
                }

                auto* efc = new ExportFuncContext{
                    instData->execEnv, instData->moduleInst, func,
                    paramCount, resultCount,
                    std::move(paramTypes), std::move(resultTypes),
                    ctx, instanceObj, &instData->poisoned,
                    exportInfo.name ? exportInfo.name : ""};
                JSValueProtect(ctx, instanceObj);

                JSObjectRef funcObj = JSObjectMake(ctx, GetExportFuncClass(), efc);
                SetFunctionPrototype(ctx, funcObj);
                JSStringRef propName = JSStringCreateWithUTF8CString(exportInfo.name);
                JSObjectSetProperty(ctx, exportsObj, propName, funcObj,
                                    kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete,
                                    nullptr);
                JSStringRelease(propName);
            }
            // Phase 2: Handle memory exports
            else if (exportInfo.kind == WASM_IMPORT_EXPORT_KIND_MEMORY) {
                wasm_memory_inst_t memInst = wasm_runtime_lookup_memory(
                    instData->moduleInst, exportInfo.name);
                if (!memInst) {
                    // Try default memory (index 0) as fallback
                    memInst = wasm_runtime_get_default_memory(instData->moduleInst);
                }
                if (memInst) {
                    JSObjectRef memObj = WrapMemoryExport(ctx, instData->moduleInst, memInst,
                                                          instanceObj);
                    JSStringRef propName = JSStringCreateWithUTF8CString(exportInfo.name);
                    JSObjectSetProperty(ctx, exportsObj, propName, memObj,
                                        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete,
                                        nullptr);
                    JSStringRelease(propName);
                }
            }
            // Phase 3: Handle table exports
            else if (exportInfo.kind == WASM_IMPORT_EXPORT_KIND_TABLE) {
                wasm_table_inst_t tableInfo{};
                if (wasm_runtime_get_export_table_inst(instData->moduleInst, exportInfo.name, &tableInfo)) {
                    JSObjectRef tblObj = WrapTableExport(
                        ctx, instData->moduleInst, tableInfo, static_cast<uint32_t>(i),
                        instData->execEnv, instanceObj);
                    JSStringRef propName = JSStringCreateWithUTF8CString(exportInfo.name);
                    JSObjectSetProperty(ctx, exportsObj, propName, tblObj,
                                        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete,
                                        nullptr);
                    JSStringRelease(propName);
                }
            }
            // Phase 3: Handle global exports
            else if (exportInfo.kind == WASM_IMPORT_EXPORT_KIND_GLOBAL) {
                wasm_global_inst_t globalInfo{};
                if (wasm_runtime_get_export_global_inst(instData->moduleInst, exportInfo.name, &globalInfo)) {
                    JSObjectRef globalObj = WrapGlobalExport(ctx, globalInfo);
                    JSStringRef propName = JSStringCreateWithUTF8CString(exportInfo.name);
                    JSObjectSetProperty(ctx, exportsObj, propName, globalObj,
                                        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete,
                                        nullptr);
                    JSStringRelease(propName);
                }
            }
        }

        return exportsObj;
    }

    // =========================================================================
    // WebAssembly.Module.exports(module) → [{name, kind}]
    // =========================================================================

    static JSValueRef WASM_ModuleExports(JSContextRef ctx, JSObjectRef /*function*/,
                                          JSObjectRef /*thisObject*/, size_t argc,
                                          const JSValueRef argv[], JSValueRef* exception) {
        if (argc < 1 || !JSValueIsObject(ctx, argv[0])) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Module.exports requires a Module argument");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        JSObjectRef moduleObj = JSValueToObject(ctx, argv[0], nullptr);
        auto* modData = static_cast<WASMModuleData*>(JSObjectGetPrivate(moduleObj));
        if (!modData || !modData->wasmModule) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Module.exports: invalid Module object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        int32_t exportCount = wasm_runtime_get_export_count(modData->wasmModule);
        JSObjectRef resultArray = JSObjectMakeArray(ctx, 0, nullptr, nullptr);

        for (int32_t i = 0; i < exportCount; i++) {
            wasm_export_t exportInfo;
            wasm_runtime_get_export_type(modData->wasmModule, i, &exportInfo);
            if (!exportInfo.name) continue;

            JSObjectRef entry = JSObjectMake(ctx, nullptr, nullptr);

            JSStringRef nameProp = JSStringCreateWithUTF8CString("name");
            JSStringRef nameVal = JSStringCreateWithUTF8CString(exportInfo.name);
            JSObjectSetProperty(ctx, entry, nameProp, JSValueMakeString(ctx, nameVal), 0, nullptr);
            JSStringRelease(nameVal);
            JSStringRelease(nameProp);

            const char* kindStr = "unknown";
            switch (exportInfo.kind) {
                case WASM_IMPORT_EXPORT_KIND_FUNC:   kindStr = "function"; break;
                case WASM_IMPORT_EXPORT_KIND_TABLE:  kindStr = "table"; break;
                case WASM_IMPORT_EXPORT_KIND_MEMORY: kindStr = "memory"; break;
                case WASM_IMPORT_EXPORT_KIND_GLOBAL: kindStr = "global"; break;
            }

            JSStringRef kindProp = JSStringCreateWithUTF8CString("kind");
            JSStringRef kindVal = JSStringCreateWithUTF8CString(kindStr);
            JSObjectSetProperty(ctx, entry, kindProp, JSValueMakeString(ctx, kindVal), 0, nullptr);
            JSStringRelease(kindVal);
            JSStringRelease(kindProp);

            JSObjectSetPropertyAtIndex(ctx, resultArray, static_cast<unsigned>(i), entry, nullptr);
        }

        return resultArray;
    }

    // =========================================================================
    // WebAssembly.Module.imports(module) → [{module, name, kind}]
    // =========================================================================

    static JSValueRef WASM_ModuleImports(JSContextRef ctx, JSObjectRef /*function*/,
                                          JSObjectRef /*thisObject*/, size_t argc,
                                          const JSValueRef argv[], JSValueRef* exception) {
        if (argc < 1 || !JSValueIsObject(ctx, argv[0])) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Module.imports requires a Module argument");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        JSObjectRef moduleObj = JSValueToObject(ctx, argv[0], nullptr);
        auto* modData = static_cast<WASMModuleData*>(JSObjectGetPrivate(moduleObj));
        if (!modData || !modData->wasmModule) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Module.imports: invalid Module object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        int32_t importCount = wasm_runtime_get_import_count(modData->wasmModule);
        JSObjectRef resultArray = JSObjectMakeArray(ctx, 0, nullptr, nullptr);

        unsigned arrayIdx = 0;
        for (int32_t i = 0; i < importCount; i++) {
            wasm_import_t importInfo;
            wasm_runtime_get_import_type(modData->wasmModule, i, &importInfo);

            JSObjectRef entry = JSObjectMake(ctx, nullptr, nullptr);

            // module
            JSStringRef moduleProp = JSStringCreateWithUTF8CString("module");
            JSStringRef moduleVal = JSStringCreateWithUTF8CString(
                importInfo.module_name ? importInfo.module_name : "");
            JSObjectSetProperty(ctx, entry, moduleProp, JSValueMakeString(ctx, moduleVal), 0, nullptr);
            JSStringRelease(moduleVal);
            JSStringRelease(moduleProp);

            // name
            JSStringRef nameProp = JSStringCreateWithUTF8CString("name");
            JSStringRef nameVal = JSStringCreateWithUTF8CString(
                importInfo.name ? importInfo.name : "");
            JSObjectSetProperty(ctx, entry, nameProp, JSValueMakeString(ctx, nameVal), 0, nullptr);
            JSStringRelease(nameVal);
            JSStringRelease(nameProp);

            // kind
            const char* kindStr = "unknown";
            switch (importInfo.kind) {
                case WASM_IMPORT_EXPORT_KIND_FUNC:   kindStr = "function"; break;
                case WASM_IMPORT_EXPORT_KIND_TABLE:  kindStr = "table"; break;
                case WASM_IMPORT_EXPORT_KIND_MEMORY: kindStr = "memory"; break;
                case WASM_IMPORT_EXPORT_KIND_GLOBAL: kindStr = "global"; break;
            }

            JSStringRef kindProp = JSStringCreateWithUTF8CString("kind");
            JSStringRef kindVal = JSStringCreateWithUTF8CString(kindStr);
            JSObjectSetProperty(ctx, entry, kindProp, JSValueMakeString(ctx, kindVal), 0, nullptr);
            JSStringRelease(kindVal);
            JSStringRelease(kindProp);

            JSObjectSetPropertyAtIndex(ctx, resultArray, arrayIdx++, entry, nullptr);
        }

        return resultArray;
    }

    // =========================================================================
    // WebAssembly.validate(bytes) → boolean
    // =========================================================================

    static JSValueRef WASM_Validate(JSContextRef ctx, JSObjectRef /*function*/,
                                     JSObjectRef /*thisObject*/, size_t argc,
                                     const JSValueRef argv[], JSValueRef* /*exc*/) {
        if (argc < 1) return JSValueMakeBoolean(ctx, false);

        auto [bytes, len] = ExtractBytes(ctx, argv[0]);
        if (!bytes || len == 0) return JSValueMakeBoolean(ctx, false);

        if (!EnsureRuntimeInitialized()) return JSValueMakeBoolean(ctx, false);

        // WAMR has no standalone validate API; attempt load + unload
        std::vector<uint8_t> bytesCopy(bytes, bytes + len);
        char errorBuf[128] = {};
        wasm_module_t mod = wasm_runtime_load(
            bytesCopy.data(), static_cast<uint32_t>(bytesCopy.size()),
            errorBuf, sizeof(errorBuf));
        bool valid = (mod != nullptr);
        if (valid) {
            wasm_runtime_unload(mod);
        }
        return JSValueMakeBoolean(ctx, valid);
    }

    // =========================================================================
    // WebAssembly.Module(bytes) constructor
    // =========================================================================

    static JSObjectRef WASM_ModuleConstructor(JSContextRef ctx, JSObjectRef /*constructor*/,
                                               size_t argc, const JSValueRef argv[],
                                               JSValueRef* exception) {
        if (argc < 1) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Module requires a BufferSource argument");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        auto [bytes, len] = ExtractBytes(ctx, argv[0]);
        if (!bytes || len == 0) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Module: invalid or empty buffer source");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        if (!EnsureRuntimeInitialized()) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Module: failed to initialize WASM runtime");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // WAMR requires the bytecode to remain alive during module lifetime,
        // so we copy it into the module data.
        auto* modData = new WASMModuleData();
        modData->bytecode.assign(bytes, bytes + len);

        char errorBuf[128] = {};
        modData->wasmModule = wasm_runtime_load(
            modData->bytecode.data(), static_cast<uint32_t>(modData->bytecode.size()),
            errorBuf, sizeof(errorBuf));

        if (!modData->wasmModule) {
            std::string errMsg = "WebAssembly.Module: compilation failed: ";
            errMsg += errorBuf;
            logger::error("[WASM] {}", errMsg);

            delete modData;

            JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        AddLiveObject();
        JSObjectRef moduleObj = JSObjectMake(ctx, GetWASMModuleClass(), modData);
        logger::info("[WASM] Module compiled successfully ({} bytes)", len);
        return moduleObj;
    }

    // =========================================================================
    // WebAssembly.Instance(module [, imports]) constructor
    // =========================================================================

    static JSObjectRef WASM_InstanceConstructor(JSContextRef ctx, JSObjectRef constructor,
                                                 size_t argc, const JSValueRef argv[],
                                                 JSValueRef* exception) {
        if (argc < 1 || !JSValueIsObject(ctx, argv[0])) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Instance requires a Module argument");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        JSObjectRef moduleObj = JSValueToObject(ctx, argv[0], nullptr);
        auto* modData = static_cast<WASMModuleData*>(JSObjectGetPrivate(moduleObj));
        if (!modData || !modData->wasmModule) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Instance: invalid Module object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // Resolve imports from the JS import object (argv[1]) before instantiation.
        // This registers native host functions with WAMR's global native symbol table.
        ImportContext importCtx;
        JSValueRef jsImports = (argc >= 2) ? argv[1] : nullptr;

        if (!ResolveImports(ctx, modData->wasmModule, jsImports, importCtx, exception)) {
            CleanupImports(importCtx);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // IMPORTANT: The module was loaded (wasm_runtime_load) before any JS import
        // functions were registered, so all func_ptr_linked fields are NULL. Now that
        // ResolveImports has registered our natives in WAMR's global symbol table,
        // we must re-resolve the module's imports so they pick up the newly-registered
        // native function pointers. Without this, instantiation copies NULL pointers
        // and calls to imported functions fail with "unlinked import function".
        if (!importCtx.modules.empty()) {
            bool resolved = wasm_runtime_resolve_symbols(modData->wasmModule);
            logger::info("[WASM] wasm_runtime_resolve_symbols returned: {}",
                         resolved ? "true" : "false");
        }

        constexpr uint32_t kStackSize = 512 * 1024;  // 512KB WASM operand stack
        constexpr uint32_t kHeapSize = 0;            // Emscripten modules manage their own heap

        char errorBuf[128] = {};
        wasm_module_inst_t moduleInst = wasm_runtime_instantiate(
            modData->wasmModule, kStackSize, kHeapSize, errorBuf, sizeof(errorBuf));

        // NOTE: Do NOT call UnregisterImportNatives() here. Although
        // func_ptr_linked is already baked into the module, WAMR also stores
        // the NativeSymbol::signature pointer on WASMFunctionImport::signature
        // and reads it at CALL TIME (in wasm_runtime_invoke_native_raw) to
        // check for pointer/string annotations. Clearing modules would destroy
        // signatureStorage and create dangling signature pointers → crash.
        // Unregistration is deferred to the instance finalizer.

        if (!moduleInst) {
            std::string errMsg = "WebAssembly.Instance: instantiation failed: ";
            errMsg += errorBuf;
            logger::error("[WASM] {}", errMsg);

            // On failure, fully clean up
            CleanupImports(importCtx);

            JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        wasm_exec_env_t execEnv = wasm_runtime_create_exec_env(moduleInst, kStackSize);
        if (!execEnv) {
            CleanupImports(importCtx);
            wasm_runtime_deinstantiate(moduleInst);
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Instance: failed to create execution environment");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // Retrieve the viewId from the constructor's stored property
        uint64_t viewId = 0;
        if (constructor) {
            JSStringRef viewIdProp = JSStringCreateWithUTF8CString("__viewId");
            JSValueRef viewIdVal = JSObjectGetProperty(ctx, constructor, viewIdProp, nullptr);
            JSStringRelease(viewIdProp);

            if (JSValueIsString(ctx, viewIdVal)) {
                JSStringRef viewIdJSStr = JSValueToStringCopy(ctx, viewIdVal, nullptr);
                size_t bufLen = JSStringGetMaximumUTF8CStringSize(viewIdJSStr);
                std::string viewIdBuf(bufLen, '\0');
                JSStringGetUTF8CString(viewIdJSStr, viewIdBuf.data(), bufLen);
                JSStringRelease(viewIdJSStr);
                viewId = static_cast<uint64_t>(std::strtoull(viewIdBuf.c_str(), nullptr, 10));
            }
        }

        auto* instData = new WASMInstanceData{moduleInst, execEnv, modData->wasmModule, viewId,
                                              std::move(importCtx)};

        // Prevent the Module JSObject from being garbage-collected while this
        // instance is alive. WAMR's module instance holds raw pointers into the
        // module's function bytecode and type info — if the Module finalizer
        // calls wasm_runtime_unload() first, those become dangling pointers.
        JSValueProtect(ctx, moduleObj);
        instData->moduleCtx = ctx;
        instData->moduleRef = moduleObj;

        // Track instance on the PrismaView for cleanup on view destroy
        {
            std::shared_lock lock(Core::viewsMutex);
            auto it = Core::views.find(viewId);
            if (it != Core::views.end()) {
                if (!it->second->wasmInstances) {
                    it->second->wasmInstances = std::make_unique<std::vector<WASMInstanceHandle>>();
                }
                it->second->wasmInstances->push_back(
                    WASMInstanceHandle{modData->wasmModule, moduleInst, execEnv});
            }
        }

        AddLiveObject();
        JSObjectRef instanceObj = JSObjectMake(ctx, GetWASMInstanceClass(), instData);

        // Build and set the .exports property
        JSObjectRef exportsObj = BuildExportsObject(ctx, instData, instanceObj);
        JSStringRef exportsProp = JSStringCreateWithUTF8CString("exports");
        JSObjectSetProperty(ctx, instanceObj, exportsProp, exportsObj,
                            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(exportsProp);

        logger::info("[WASM] Instance created for view {}", viewId);
        return instanceObj;
    }

    // =========================================================================
    // Helper: retrieve the WebAssembly.Instance constructor from JS global
    // =========================================================================

    static JSObjectRef GetInstanceCtor(JSContextRef ctx) {
        JSObjectRef global = JSContextGetGlobalObject(ctx);
        JSStringRef waName = JSStringCreateWithUTF8CString("WebAssembly");
        JSValueRef waVal = JSObjectGetProperty(ctx, global, waName, nullptr);
        JSStringRelease(waName);
        JSObjectRef waObj = JSValueToObject(ctx, waVal, nullptr);
        if (!waObj) return nullptr;

        JSStringRef instName = JSStringCreateWithUTF8CString("Instance");
        JSValueRef instCtorVal = JSObjectGetProperty(ctx, waObj, instName, nullptr);
        JSStringRelease(instName);
        return JSValueToObject(ctx, instCtorVal, nullptr);
    }

    // =========================================================================
    // WebAssembly.compile(bytes) → Promise<Module>
    // =========================================================================

    static JSValueRef WASM_Compile(JSContextRef ctx, JSObjectRef /*function*/,
                                    JSObjectRef /*thisObject*/, size_t argc,
                                    const JSValueRef argv[], JSValueRef* exception) {
        if (argc < 1) {
            return RejectPromise(ctx, "WebAssembly.compile requires a BufferSource argument",
                                 "CompileError");
        }

        // Synchronously compile, then wrap in resolved Promise
        JSObjectRef moduleObj = WASM_ModuleConstructor(ctx, nullptr, argc, argv, exception);
        if (exception && *exception) {
            return ExceptionToRejectedPromise(ctx, exception);
        }

        return ResolvePromise(ctx, moduleObj);
    }

    // =========================================================================
    // WebAssembly.instantiate(bufferOrModule [, imports])
    //   - If first arg is BufferSource: → Promise<{module, instance}>
    //   - If first arg is Module: → Promise<Instance>
    // =========================================================================

    static JSValueRef WASM_Instantiate(JSContextRef ctx, JSObjectRef /*function*/,
                                        JSObjectRef /*thisObject*/, size_t argc,
                                        const JSValueRef argv[], JSValueRef* exception) {
        if (argc < 1) {
            return RejectPromise(ctx, "WebAssembly.instantiate requires at least one argument",
                                 "CompileError");
        }

        // Check if first arg is a Module (has our Module class private data)
        JSObjectRef firstArg = JSValueToObject(ctx, argv[0], nullptr);
        bool isModule = false;
        if (firstArg) {
            auto* maybeModData = static_cast<WASMModuleData*>(JSObjectGetPrivate(firstArg));
            if (maybeModData && maybeModData->wasmModule) {
                isModule = true;
            }
        }

        JSObjectRef instCtor = GetInstanceCtor(ctx);

        if (isModule) {
            // instantiate(module, imports) → Promise<Instance>
            JSObjectRef instanceObj = WASM_InstanceConstructor(ctx, instCtor, argc, argv, exception);
            if (exception && *exception) {
                return ExceptionToRejectedPromise(ctx, exception);
            }
            return ResolvePromise(ctx, instanceObj);
        }

        // instantiate(bytes, imports) → Promise<{module, instance}>
        // First compile the module
        JSValueRef compileException = nullptr;
        JSObjectRef moduleObj = WASM_ModuleConstructor(ctx, nullptr, 1, argv, &compileException);
        if (compileException) {
            return ExceptionToRejectedPromise(ctx, &compileException);
        }

        // Then instantiate
        size_t instArgc;
        JSValueRef instArgsWithImport[2];
        JSValueRef instArgsNoImport[1];
        const JSValueRef* instArgs;

        if (argc >= 2) {
            instArgsWithImport[0] = moduleObj;
            instArgsWithImport[1] = argv[1];
            instArgs = instArgsWithImport;
            instArgc = 2;
        } else {
            instArgsNoImport[0] = moduleObj;
            instArgs = instArgsNoImport;
            instArgc = 1;
        }

        JSValueRef instException = nullptr;
        JSObjectRef instanceObj = WASM_InstanceConstructor(
            ctx, instCtor, instArgc, instArgs, &instException);
        if (instException) {
            return ExceptionToRejectedPromise(ctx, &instException);
        }

        // Build result object: {module, instance}
        JSObjectRef resultObj = JSObjectMake(ctx, nullptr, nullptr);
        JSStringRef modProp = JSStringCreateWithUTF8CString("module");
        JSObjectSetProperty(ctx, resultObj, modProp, moduleObj, 0, nullptr);
        JSStringRelease(modProp);
        JSStringRef instProp = JSStringCreateWithUTF8CString("instance");
        JSObjectSetProperty(ctx, resultObj, instProp, instanceObj, 0, nullptr);
        JSStringRelease(instProp);

        return ResolvePromise(ctx, resultObj);
    }

    // =========================================================================
    // InjectWASMBindings: called from OnWindowObjectReady
    // =========================================================================

    void InjectWASMBindings(JSContextRef jsCtx, uint64_t viewId) {
        JSObjectRef globalObj = JSContextGetGlobalObject(jsCtx);

        // Install Promise helpers on the global object
        const char* promiseHelperScript =
            "window.__prismaWASMResolve = function(v) { return Promise.resolve(v); };"
            "window.__prismaWASMReject  = function(e) { return Promise.reject(e); };";
        JSStringRef helperStr = JSStringCreateWithUTF8CString(promiseHelperScript);
        JSEvaluateScript(jsCtx, helperStr, nullptr, nullptr, 0, nullptr);
        JSStringRelease(helperStr);

        // Store viewId string for later retrieval by constructors
        std::string viewIdStr = std::to_string(viewId);

        // Create the WebAssembly global object
        JSObjectRef wasmObj = JSObjectMake(jsCtx, nullptr, nullptr);

        // -- validate --
        JSStringRef validateName = JSStringCreateWithUTF8CString("validate");
        JSObjectRef validateFunc = JSObjectMakeFunctionWithCallback(jsCtx, validateName, WASM_Validate);
        JSObjectSetProperty(jsCtx, wasmObj, validateName, validateFunc,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(validateName);

        // -- compile --
        JSStringRef compileName = JSStringCreateWithUTF8CString("compile");
        JSObjectRef compileFunc = JSObjectMakeFunctionWithCallback(jsCtx, compileName, WASM_Compile);
        JSObjectSetProperty(jsCtx, wasmObj, compileName, compileFunc,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(compileName);

        // -- instantiate --
        JSStringRef instantiateName = JSStringCreateWithUTF8CString("instantiate");
        JSObjectRef instantiateFunc = JSObjectMakeFunctionWithCallback(
            jsCtx, instantiateName, WASM_Instantiate);
        JSObjectSetProperty(jsCtx, wasmObj, instantiateName, instantiateFunc,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(instantiateName);

        // -- Module constructor --
        JSStringRef moduleName = JSStringCreateWithUTF8CString("Module");
        JSObjectRef moduleCtor = JSObjectMakeConstructor(jsCtx, GetWASMModuleClass(),
                                                          WASM_ModuleConstructor);

        // -- Module.exports() and Module.imports() static methods --
        JSStringRef modExportsName = JSStringCreateWithUTF8CString("exports");
        JSObjectRef modExportsFunc = JSObjectMakeFunctionWithCallback(jsCtx, modExportsName, WASM_ModuleExports);
        JSObjectSetProperty(jsCtx, moduleCtor, modExportsName, modExportsFunc,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(modExportsName);

        JSStringRef modImportsName = JSStringCreateWithUTF8CString("imports");
        JSObjectRef modImportsFunc = JSObjectMakeFunctionWithCallback(jsCtx, modImportsName, WASM_ModuleImports);
        JSObjectSetProperty(jsCtx, moduleCtor, modImportsName, modImportsFunc,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(modImportsName);

        JSObjectSetProperty(jsCtx, wasmObj, moduleName, moduleCtor,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(moduleName);

        // -- Instance constructor --
        JSStringRef instanceName = JSStringCreateWithUTF8CString("Instance");
        JSObjectRef instanceCtor = JSObjectMakeConstructor(jsCtx, GetWASMInstanceClass(),
                                                            WASM_InstanceConstructor);
        // Store viewId on the Instance constructor so it can be retrieved during instantiation
        JSStringRef viewIdPropStr = JSStringCreateWithUTF8CString("__viewId");
        JSStringRef viewIdJSStr = JSStringCreateWithUTF8CString(viewIdStr.c_str());
        JSObjectSetProperty(jsCtx, instanceCtor, viewIdPropStr,
                            JSValueMakeString(jsCtx, viewIdJSStr),
                            kJSPropertyAttributeDontEnum | kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(viewIdJSStr);
        JSStringRelease(viewIdPropStr);
        JSObjectSetProperty(jsCtx, wasmObj, instanceName, instanceCtor,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(instanceName);

        // -- Memory constructor --
        JSStringRef memoryName = JSStringCreateWithUTF8CString("Memory");
        JSObjectRef memoryCtor = JSObjectMakeConstructor(jsCtx, GetWASMMemoryClass(),
                                                          WASM_MemoryConstructor);
        JSObjectSetProperty(jsCtx, wasmObj, memoryName, memoryCtor,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(memoryName);

        // -- Table constructor --
        JSStringRef tableName = JSStringCreateWithUTF8CString("Table");
        JSObjectRef tableCtor = JSObjectMakeConstructor(jsCtx, GetWASMTableClass(),
                                                         WASM_TableConstructor);
        JSObjectSetProperty(jsCtx, wasmObj, tableName, tableCtor,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(tableName);

        // -- Global constructor --
        JSStringRef globalName = JSStringCreateWithUTF8CString("Global");
        JSObjectRef globalCtor = JSObjectMakeConstructor(jsCtx, GetWASMGlobalClass(),
                                                          WASM_GlobalConstructor);
        JSObjectSetProperty(jsCtx, wasmObj, globalName, globalCtor,
                            kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(globalName);

        // -- Error types (minimal constructors) --
        const char* errorScript =
            "(function(WA) {"
            "  WA.CompileError = class CompileError extends Error {"
            "    constructor(msg) { super(msg); this.name = 'CompileError'; }"
            "  };"
            "  WA.LinkError = class LinkError extends Error {"
            "    constructor(msg) { super(msg); this.name = 'LinkError'; }"
            "  };"
            "  WA.RuntimeError = class RuntimeError extends Error {"
            "    constructor(msg) { super(msg); this.name = 'RuntimeError'; }"
            "  };"
            "})";
        JSStringRef errorScriptStr = JSStringCreateWithUTF8CString(errorScript);
        JSValueRef errorSetupFunc = JSEvaluateScript(jsCtx, errorScriptStr, nullptr, nullptr, 0, nullptr);
        JSStringRelease(errorScriptStr);

        if (JSValueIsObject(jsCtx, errorSetupFunc)) {
            JSObjectRef errorSetupObj = JSValueToObject(jsCtx, errorSetupFunc, nullptr);
            if (errorSetupObj) {
                JSValueRef setupArgs[] = {wasmObj};
                JSObjectCallAsFunction(jsCtx, errorSetupObj, nullptr, 1, setupArgs, nullptr);
            }
        }

        // Set WebAssembly on global
        JSStringRef wasmName = JSStringCreateWithUTF8CString("WebAssembly");
        JSObjectSetProperty(jsCtx, globalObj, wasmName, wasmObj,
                            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, nullptr);
        JSStringRelease(wasmName);

        // -- compileStreaming / instantiateStreaming polyfills --
        // These delegate to fetch().arrayBuffer() then the synchronous compile/instantiate.
        const char* streamingScript =
            "(function(WA) {"
            "  WA.compileStreaming = function(source) {"
            "    return Promise.resolve(source)"
            "      .then(function(response) {"
            "        if (typeof response.ok !== 'undefined' && !response.ok)"
            "          throw new TypeError('Response has non-OK status: ' + response.status);"
            "        return response.arrayBuffer();"
            "      })"
            "      .then(function(bytes) {"
            "        return WA.compile(bytes);"
            "      });"
            "  };"
            "  WA.instantiateStreaming = function(source, importObject) {"
            "    return Promise.resolve(source)"
            "      .then(function(response) {"
            "        if (typeof response.ok !== 'undefined' && !response.ok)"
            "          throw new TypeError('Response has non-OK status: ' + response.status);"
            "        return response.arrayBuffer();"
            "      })"
            "      .then(function(bytes) {"
            "        return WA.instantiate(bytes, importObject);"
            "      });"
            "  };"
            "})(WebAssembly);";
        JSStringRef streamingStr = JSStringCreateWithUTF8CString(streamingScript);
        JSEvaluateScript(jsCtx, streamingStr, nullptr, nullptr, 0, nullptr);
        JSStringRelease(streamingStr);

        logger::info("[WASM] WebAssembly bindings injected into JS context for view {}", viewId);
    }

}  // namespace PrismaUI::WASM
