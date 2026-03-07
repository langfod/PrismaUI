#include "WASMBridgeObjects.h"
#include "WASMBridge.h"
#include "WASMRuntime.h"

#include <JavaScriptCore/JavaScript.h>
#include <wasm_export.h>

#include <cmath>
#include <cstring>
#include <string>

// Forward-declare WAMR internal table enlarge function.
// In interpreter mode (our configuration), wasm_module_inst_t (WASMModuleInstanceCommon*)
// points to a WASMModuleInstance. The internal function operates on that type.
// We cast at the call site. This is pinned to WAMR 2.2.0 via FetchContent.
struct WASMModuleInstance;
extern "C" bool wasm_enlarge_table(WASMModuleInstance* module_inst, uint32_t table_idx,
                                   uint32_t inc_entries, uintptr_t init_val);

namespace PrismaUI::WASM {

    // =========================================================================
    //  MEMORY
    // =========================================================================

    // --- Memory finalizer ---

    static void WASMMemoryFinalize(JSObjectRef obj) {
        auto* data = static_cast<WASMMemoryData*>(JSObjectGetPrivate(obj));
        if (data) {
            // [064] Unprotect the cached ArrayBuffer before freeing its owner
            if (data->cachedBuffer && data->ctx) {
                JSValueUnprotect(data->ctx, data->cachedBuffer);
                data->cachedBuffer = nullptr;
            }
            if (data->instanceRef && data->ctx) {
                JSValueUnprotect(data->ctx, data->instanceRef);
            }
            // [060] For standalone memory, deinstantiate and unload the miniModule
            if (data->ownsMemory && data->moduleInst) {
                wasm_runtime_deinstantiate(data->moduleInst);
            }
            if (data->miniModule) {
                wasm_runtime_unload(data->miniModule);
            }
            delete data;
            RemoveLiveObject();
        }
    }

    // --- Memory.prototype.grow(delta) -> oldPageCount ---

    static JSValueRef WASM_MemoryGrow(JSContextRef ctx, JSObjectRef /*function*/,
                                       JSObjectRef thisObject, size_t argc,
                                       const JSValueRef argv[], JSValueRef* exception) {
        auto* mem = static_cast<WASMMemoryData*>(JSObjectGetPrivate(thisObject));
        if (!mem || !mem->memoryInst) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("Memory.grow: invalid memory object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        if (argc < 1) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("Memory.grow: requires delta argument");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        // [062] Validate delta before casting — NaN/negative/overflow are UB on uint64_t
        double dDelta = JSValueToNumber(ctx, argv[0], nullptr);
        if (std::isnan(dDelta) || dDelta < 0.0 || dDelta > 65536.0) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Memory.grow: delta must be a non-negative integer within WASM page limits");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }
        uint64_t delta = static_cast<uint64_t>(dDelta);
        uint64_t oldPages = wasm_memory_get_cur_page_count(mem->memoryInst);

        if (!wasm_memory_enlarge(mem->memoryInst, delta)) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Memory.grow: failed to grow memory (may exceed maximum)");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        // [064] Unprotect then invalidate the cached ArrayBuffer — the base pointer may have changed
        if (mem->cachedBuffer && mem->ctx) {
            JSValueUnprotect(mem->ctx, mem->cachedBuffer);
        }
        mem->cachedBuffer = nullptr;

        logger::info("[WASM] Memory.grow: {} -> {} pages", oldPages, oldPages + delta);
        return JSValueMakeNumber(ctx, static_cast<double>(oldPages));
    }

    // --- Memory.prototype.buffer (getter) ---

    static JSValueRef WASM_MemoryGetBuffer(JSContextRef ctx, JSObjectRef object,
                                            JSStringRef /*propertyName*/, JSValueRef* exception) {
        auto* mem = static_cast<WASMMemoryData*>(JSObjectGetPrivate(object));
        if (!mem || !mem->memoryInst) {
            return JSValueMakeUndefined(ctx);
        }

        void* base = wasm_memory_get_base_address(mem->memoryInst);
        uint64_t curPages = wasm_memory_get_cur_page_count(mem->memoryInst);
        uint64_t bytesPerPage = wasm_memory_get_bytes_per_page(mem->memoryInst);
        size_t size = static_cast<size_t>(curPages * bytesPerPage);

        if (!base || size == 0) {
            return JSValueMakeUndefined(ctx);
        }

        // [064] Unprotect and invalidate if WAMR's base pointer or size changed
        if (mem->cachedBuffer && (mem->cachedBase != base || mem->cachedSize != size)) {
            static bool loggedInvalidation = false;
            if (!loggedInvalidation) {
                logger::info("[WASM] Memory.buffer: base pointer changed ({} -> {}, size {} -> {}) — recreating ArrayBuffer",
                    mem->cachedBase, base, mem->cachedSize, size);
                loggedInvalidation = true;
            }
            JSValueUnprotect(ctx, mem->cachedBuffer);
            mem->cachedBuffer = nullptr;
        }

        // Return cached buffer if still valid
        if (mem->cachedBuffer) {
            return mem->cachedBuffer;
        }

        // Create an ArrayBuffer that wraps WAMR's memory directly (zero-copy).
        // The deallocator is a no-op because WAMR owns the memory.
        JSObjectRef buffer = JSObjectMakeArrayBufferWithBytesNoCopy(
            ctx, base, size,
            [](void* /*bytes*/, void* /*deallocatorContext*/) {
                // No-op: WAMR owns this memory. The ArrayBuffer is just a view.
            },
            nullptr, exception);

        if (buffer && !(exception && *exception)) {
            // [064] Protect the buffer and store ctx so the finalizer can unprotect
            JSValueProtect(ctx, buffer);
            if (!mem->ctx) mem->ctx = ctx;  // Capture ctx for standalone memory finalizer
            mem->cachedBuffer = buffer;
            mem->cachedBase = base;
            mem->cachedSize = size;

            // Verify the ArrayBuffer's data pointer matches WAMR's base
            void* abPtr = JSObjectGetArrayBufferBytesPtr(ctx, buffer, nullptr);
            static bool loggedBufferCreation = false;
            if (!loggedBufferCreation) {
                // Also sample a few bytes from WAMR memory directly
                auto* raw = static_cast<uint8_t*>(base);
                bool wamrAllZero = true;
                for (size_t i = 0; i < 256 && i < size; i++) {
                    if (raw[i] != 0) { wamrAllZero = false; break; }
                }
                logger::info("[WASM] Memory.buffer created: WAMRbase={} ABptr={} match={} size={} wamrFirst256AllZero={}",
                    base, abPtr, (base == abPtr), size, wamrAllZero);
                loggedBufferCreation = true;
            }
        }

        return buffer ? buffer : JSValueMakeUndefined(ctx);
    }

    // --- Memory.prototype.byteLength (getter) ---

    static JSValueRef WASM_MemoryGetByteLength(JSContextRef ctx, JSObjectRef object,
                                                JSStringRef /*propertyName*/, JSValueRef* /*exception*/) {
        auto* mem = static_cast<WASMMemoryData*>(JSObjectGetPrivate(object));
        if (!mem || !mem->memoryInst) {
            return JSValueMakeNumber(ctx, 0);
        }

        uint64_t curPages = wasm_memory_get_cur_page_count(mem->memoryInst);
        uint64_t bytesPerPage = wasm_memory_get_bytes_per_page(mem->memoryInst);
        return JSValueMakeNumber(ctx, static_cast<double>(curPages * bytesPerPage));
    }

    // --- GetWASMMemoryClass ---

    JSClassRef GetWASMMemoryClass() {
        static JSClassRef cls = []() {
            static JSStaticValue kMemoryValues[] = {
                {"buffer", WASM_MemoryGetBuffer, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
                {"byteLength", WASM_MemoryGetByteLength, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
                {nullptr, nullptr, nullptr, 0}
            };

            static JSStaticFunction kMemoryFunctions[] = {
                {"grow", WASM_MemoryGrow, kJSPropertyAttributeDontDelete},
                {nullptr, nullptr, 0}
            };

            JSClassDefinition def{};
            def.className = "WebAssembly.Memory";
            def.finalize = WASMMemoryFinalize;
            def.staticValues = kMemoryValues;
            def.staticFunctions = kMemoryFunctions;
            return JSClassCreate(&def);
        }();
        return cls;
    }

    // --- WebAssembly.Memory constructor ---

    JSObjectRef WASM_MemoryConstructor(JSContextRef ctx, JSObjectRef /*constructor*/,
                                       size_t argc, const JSValueRef argv[],
                                       JSValueRef* exception) {
        if (argc < 1 || !JSValueIsObject(ctx, argv[0])) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Memory requires a descriptor object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        JSObjectRef descriptor = JSValueToObject(ctx, argv[0], nullptr);

        // Read 'initial' (required)
        JSStringRef initialProp = JSStringCreateWithUTF8CString("initial");
        JSValueRef initialVal = JSObjectGetProperty(ctx, descriptor, initialProp, nullptr);
        JSStringRelease(initialProp);

        if (!JSValueIsNumber(ctx, initialVal)) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Memory: 'initial' is required and must be a number");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        uint32_t initial = static_cast<uint32_t>(JSValueToNumber(ctx, initialVal, nullptr));

        // Read 'maximum' (optional)
        uint32_t maximum = 65536;  // WASM spec max: 4GB / 64KB per page
        JSStringRef maximumProp = JSStringCreateWithUTF8CString("maximum");
        JSValueRef maximumVal = JSObjectGetProperty(ctx, descriptor, maximumProp, nullptr);
        JSStringRelease(maximumProp);

        if (JSValueIsNumber(ctx, maximumVal)) {
            maximum = static_cast<uint32_t>(JSValueToNumber(ctx, maximumVal, nullptr));
        }

        if (initial > maximum) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Memory: 'initial' must not exceed 'maximum'");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        if (!EnsureRuntimeInitialized()) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Memory: failed to initialize WASM runtime");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // Standalone Memory objects are created by compiling+instantiating a
        // minimal WASM module with the requested memory. This lets WAMR manage
        // the memory allocation natively, and the memory can later be imported
        // into other modules.
        std::string memName = "memory";
        std::vector<uint8_t> wasmBytes;

        // Helper to emit a LEB128 u32
        auto emitLEB128 = [&wasmBytes](uint32_t val) {
            do {
                uint8_t byte = val & 0x7F;
                val >>= 7;
                if (val != 0) byte |= 0x80;
                wasmBytes.push_back(byte);
            } while (val != 0);
        };

        // Magic + version
        wasmBytes.insert(wasmBytes.end(), {0x00, 0x61, 0x73, 0x6D});  // \0asm
        wasmBytes.insert(wasmBytes.end(), {0x01, 0x00, 0x00, 0x00});  // version 1

        // Memory section (section id = 5)
        {
            std::vector<uint8_t> memSection;
            auto emitLEB = [&memSection](uint32_t val) {
                do {
                    uint8_t byte = val & 0x7F;
                    val >>= 7;
                    if (val != 0) byte |= 0x80;
                    memSection.push_back(byte);
                } while (val != 0);
            };
            emitLEB(1);         // count: 1 memory
            memSection.push_back(0x01);  // flags: has_max
            emitLEB(initial);
            emitLEB(maximum);

            wasmBytes.push_back(0x05);  // section id: memory
            emitLEB128(static_cast<uint32_t>(memSection.size()));
            wasmBytes.insert(wasmBytes.end(), memSection.begin(), memSection.end());
        }

        // Export section (section id = 7)
        {
            std::vector<uint8_t> expSection;
            auto emitLEB = [&expSection](uint32_t val) {
                do {
                    uint8_t byte = val & 0x7F;
                    val >>= 7;
                    if (val != 0) byte |= 0x80;
                    expSection.push_back(byte);
                } while (val != 0);
            };
            emitLEB(1);  // count: 1 export
            emitLEB(static_cast<uint32_t>(memName.size()));
            expSection.insert(expSection.end(), memName.begin(), memName.end());
            expSection.push_back(0x02);  // kind: memory
            emitLEB(0);                  // index: 0

            wasmBytes.push_back(0x07);  // section id: export
            emitLEB128(static_cast<uint32_t>(expSection.size()));
            wasmBytes.insert(wasmBytes.end(), expSection.begin(), expSection.end());
        }

        // Load the mini module
        char errorBuf[128] = {};
        wasm_module_t miniModule = wasm_runtime_load(
            wasmBytes.data(), static_cast<uint32_t>(wasmBytes.size()),
            errorBuf, sizeof(errorBuf));

        if (!miniModule) {
            std::string errMsg = "WebAssembly.Memory: internal module load failed: ";
            errMsg += errorBuf;
            logger::error("[WASM] {}", errMsg);

            JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // Instantiate with enough stack/heap for the memory
        wasm_module_inst_t miniInst = wasm_runtime_instantiate(
            miniModule, 4096, 0, errorBuf, sizeof(errorBuf));

        if (!miniInst) {
            std::string errMsg = "WebAssembly.Memory: internal instantiation failed: ";
            errMsg += errorBuf;
            logger::error("[WASM] {}", errMsg);
            wasm_runtime_unload(miniModule);

            JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        wasm_memory_inst_t memInst = wasm_runtime_get_default_memory(miniInst);
        if (!memInst) {
            logger::error("[WASM] WebAssembly.Memory: no default memory after instantiation");
            wasm_runtime_deinstantiate(miniInst);
            wasm_runtime_unload(miniModule);

            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Memory: failed to create memory");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // [060] Store ctx for finalizer use (cachedBuffer unprotect) and miniModule for cleanup
        auto* memData = new WASMMemoryData{miniInst, memInst, nullptr, nullptr, 0, true, ctx, nullptr, miniModule};

        AddLiveObject();
        JSObjectRef memObj = JSObjectMake(ctx, GetWASMMemoryClass(), memData);

        logger::info("[WASM] Memory created: initial={}, maximum={} pages", initial, maximum);
        return memObj;
    }

    // --- WrapMemoryExport ---

    JSObjectRef WrapMemoryExport(JSContextRef ctx, wasm_module_inst_t moduleInst,
                                  wasm_memory_inst_t memoryInst,
                                  JSObjectRef instanceObj) {
        auto* memData = new WASMMemoryData{moduleInst, memoryInst, nullptr, nullptr, 0, false,
                                           ctx, instanceObj};
        if (instanceObj) {
            JSValueProtect(ctx, instanceObj);
        }

        AddLiveObject();
        return JSObjectMake(ctx, GetWASMMemoryClass(), memData);
    }

    // =========================================================================
    //  TABLE
    // =========================================================================

    // --- Table finalizer ---

    static void WASMTableFinalize(JSObjectRef obj) {
        auto* data = static_cast<WASMTableData*>(JSObjectGetPrivate(obj));
        if (data) {
            if (data->instanceRef && data->ctx) {
                JSValueUnprotect(data->ctx, data->instanceRef);
            }
            // [061] For standalone table, deinstantiate and unload the miniModule
            if (data->ownsTable && data->moduleInst) {
                wasm_runtime_deinstantiate(data->moduleInst);
            }
            if (data->miniModule) {
                wasm_runtime_unload(data->miniModule);
            }
            delete data;
            RemoveLiveObject();
        }
    }

    // --- Table.prototype.length (getter) ---

    static JSValueRef WASM_TableGetLength(JSContextRef ctx, JSObjectRef object,
                                           JSStringRef /*propertyName*/, JSValueRef* /*exception*/) {
        auto* tbl = static_cast<WASMTableData*>(JSObjectGetPrivate(object));
        if (!tbl || !tbl->moduleInst) {
            return JSValueMakeNumber(ctx, 0);
        }

        // Re-fetch table info to get current size (may have changed via WASM-side grow).
        // Only standalone tables (ownsTable) have a known export name ("table").
        // For export-wrapped tables, the cached info is updated after Table.grow().
        if (tbl->ownsTable) {
            wasm_table_inst_t freshInfo{};
            if (wasm_runtime_get_export_table_inst(tbl->moduleInst, "table", &freshInfo)) {
                tbl->tableInfo.cur_size = freshInfo.cur_size;
                tbl->tableInfo.max_size = freshInfo.max_size;
            }
        }

        return JSValueMakeNumber(ctx, static_cast<double>(tbl->tableInfo.cur_size));
    }

    // --- Table.prototype.get(index) -> Function|null ---

    static JSValueRef WASM_TableGet(JSContextRef ctx, JSObjectRef /*function*/,
                                     JSObjectRef thisObject, size_t argc,
                                     const JSValueRef argv[], JSValueRef* exception) {
        auto* tbl = static_cast<WASMTableData*>(JSObjectGetPrivate(thisObject));
        if (!tbl || !tbl->moduleInst) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("Table.get: invalid table object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        if (argc < 1) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("Table.get: requires index argument");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        uint32_t index = static_cast<uint32_t>(JSValueToNumber(ctx, argv[0], nullptr));

        if (index >= tbl->tableInfo.cur_size) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Table.get: index out of bounds");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        // Only funcref tables are supported for now
        if (tbl->tableInfo.elem_kind != WASM_FUNCREF) {
            return JSValueMakeNull(ctx);
        }

        wasm_function_inst_t func = wasm_table_get_func_inst(
            tbl->moduleInst, &tbl->tableInfo, index);

        if (!func) {
            return JSValueMakeNull(ctx);
        }

        // Wrap as a callable JS function — reuse the ExportFuncContext pattern
        // We need the ExportFuncClass from WASMBridge.cpp. Since we can't access
        // it directly, we create a simple callable wrapper via JSObjectMakeFunctionWithCallback.
        // The function captures are stored in a simple lambda-like struct.

        // We need exec_env to call the function. If we don't have one, return null.
        if (!tbl->execEnv) {
            return JSValueMakeNull(ctx);
        }

        // Get function type info
        uint32_t paramCount = wasm_func_get_param_count(func, tbl->moduleInst);
        uint32_t resultCount = wasm_func_get_result_count(func, tbl->moduleInst);
        std::vector<wasm_valkind_t> paramTypes(paramCount);
        std::vector<wasm_valkind_t> resultTypes(resultCount);
        if (paramCount > 0) {
            wasm_func_get_param_types(func, tbl->moduleInst, paramTypes.data());
        }
        if (resultCount > 0) {
            wasm_func_get_result_types(func, tbl->moduleInst, resultTypes.data());
        }

        // Store context as private data on a function object
        struct TableFuncContext {
            wasm_exec_env_t execEnv;
            wasm_module_inst_t moduleInst;
            wasm_function_inst_t wasmFunc;
            uint32_t paramCount;
            uint32_t resultCount;
            std::vector<wasm_valkind_t> paramTypes;
            std::vector<wasm_valkind_t> resultTypes;
            JSContextRef ctx;
            JSObjectRef instanceRef;  // JSValueProtect'd
        };

        auto* tfc = new TableFuncContext{
            tbl->execEnv, tbl->moduleInst, func,
            paramCount, resultCount,
            std::move(paramTypes), std::move(resultTypes),
            ctx, tbl->instanceRef};
        if (tbl->instanceRef) {
            JSValueProtect(ctx, tbl->instanceRef);
        }

        // Create a JSC class for this table function wrapper
        static JSClassRef s_tableFuncClass = nullptr;
        if (!s_tableFuncClass) {
            JSClassDefinition def{};
            def.className = "WebAssembly.TableFunction";
            def.finalize = [](JSObjectRef obj) {
                auto* tfc = static_cast<TableFuncContext*>(JSObjectGetPrivate(obj));
                if (tfc) {
                    if (tfc->instanceRef && tfc->ctx) {
                        JSValueUnprotect(tfc->ctx, tfc->instanceRef);
                    }
                    delete tfc;
                }
            };
            def.callAsFunction = [](JSContextRef ctx, JSObjectRef function,
                                    JSObjectRef /*thisObject*/, size_t argc,
                                    const JSValueRef argv[],
                                    JSValueRef* exception) -> JSValueRef {
                auto* tfc = static_cast<TableFuncContext*>(JSObjectGetPrivate(function));
                if (!tfc || !tfc->execEnv || !tfc->wasmFunc) {
                    return JSValueMakeUndefined(ctx);
                }

                uint32_t maxSlots = (tfc->paramCount + tfc->resultCount) * 2;
                if (maxSlots < 2) maxSlots = 2;

                constexpr uint32_t kStackSlots = 32;
                uint32_t stackBuf[kStackSlots] = {};
                std::vector<uint32_t> heapBuf;
                uint32_t* wasmArgv = stackBuf;
                if (maxSlots > kStackSlots) {
                    heapBuf.resize(maxSlots, 0);
                    wasmArgv = heapBuf.data();
                }

                uint32_t slotIdx = 0;
                for (uint32_t i = 0; i < tfc->paramCount && i < static_cast<uint32_t>(argc); i++) {
                    wasm_valkind_t paramType = (i < tfc->paramTypes.size()) ?
                        tfc->paramTypes[i] : WASM_I32;

                    if (paramType == WASM_F64) {
                        double val = JSValueToNumber(ctx, argv[i], nullptr);
                        memcpy(&wasmArgv[slotIdx], &val, sizeof(double));
                        slotIdx += 2;
                    } else if (paramType == WASM_F32) {
                        float val = static_cast<float>(JSValueToNumber(ctx, argv[i], nullptr));
                        memcpy(&wasmArgv[slotIdx], &val, sizeof(float));
                        slotIdx += 1;
                    } else if (paramType == WASM_I64) {
                        int64_t val = static_cast<int64_t>(JSValueToNumber(ctx, argv[i], nullptr));
                        memcpy(&wasmArgv[slotIdx], &val, sizeof(int64_t));
                        slotIdx += 2;
                    } else {
                        wasmArgv[slotIdx] = static_cast<uint32_t>(
                            static_cast<int32_t>(JSValueToNumber(ctx, argv[i], nullptr)));
                        slotIdx += 1;
                    }
                }

                if (!SEHCallWasm(tfc->execEnv, tfc->wasmFunc,
                                 slotIdx, wasmArgv)) {
                    const char* exceptionMsg = wasm_runtime_get_exception(
                        wasm_runtime_get_module_inst(tfc->execEnv));
                    std::string errStr = exceptionMsg ? exceptionMsg : "WASM runtime error";
                    logger::error("[WASM] Table function call failed: {}", errStr);

                    JSStringRef errJSStr = JSStringCreateWithUTF8CString(errStr.c_str());
                    JSValueRef errVal = JSValueMakeString(ctx, errJSStr);
                    JSStringRelease(errJSStr);
                    *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
                    return JSValueMakeUndefined(ctx);
                }

                if (tfc->resultCount == 0) {
                    return JSValueMakeUndefined(ctx);
                }

                wasm_valkind_t resultType = tfc->resultTypes.empty() ?
                    WASM_I32 : tfc->resultTypes[0];

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
                    return JSValueMakeNumber(ctx, static_cast<double>(
                        static_cast<int32_t>(wasmArgv[0])));
                }
            };
            s_tableFuncClass = JSClassCreate(&def);
        }

        JSObjectRef tableFuncObj = JSObjectMake(ctx, s_tableFuncClass, tfc);

        // Inherit from Function.prototype so .apply()/.call()/.bind() work
        JSObjectRef global = JSContextGetGlobalObject(ctx);
        JSStringRef funcName = JSStringCreateWithUTF8CString("Function");
        JSValueRef funcVal = JSObjectGetProperty(ctx, global, funcName, nullptr);
        JSStringRelease(funcName);
        if (JSValueIsObject(ctx, funcVal)) {
            JSObjectRef funcCtor = JSValueToObject(ctx, funcVal, nullptr);
            JSStringRef protoName = JSStringCreateWithUTF8CString("prototype");
            JSValueRef protoVal = JSObjectGetProperty(ctx, funcCtor, protoName, nullptr);
            JSStringRelease(protoName);
            JSObjectSetPrototype(ctx, tableFuncObj, protoVal);
        }

        return tableFuncObj;
    }

    // --- Table.prototype.set(index, value) ---

    static JSValueRef WASM_TableSet(JSContextRef ctx, JSObjectRef /*function*/,
                                     JSObjectRef thisObject, size_t argc,
                                     const JSValueRef argv[], JSValueRef* exception) {
        auto* tbl = static_cast<WASMTableData*>(JSObjectGetPrivate(thisObject));
        if (!tbl || !tbl->moduleInst) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("Table.set: invalid table object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        if (argc < 1) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("Table.set: requires index argument");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        uint32_t index = static_cast<uint32_t>(JSValueToNumber(ctx, argv[0], nullptr));

        if (index >= tbl->tableInfo.cur_size) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Table.set: index out of bounds");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        // Setting table elements to null (clearing) is the main supported case.
        // Setting to a JS function would require creating a WAMR function from JS,
        // which is not straightforward. For now, only null is supported to match
        // the common use case of clearing table entries.
        if (argc < 2 || JSValueIsNull(ctx, argv[1]) || JSValueIsUndefined(ctx, argv[1])) {
            // [065] Use kNullRef (0xFFFFFFFF) — the same sentinel Table.grow uses for new entries
            constexpr uint32_t kNullRef = 0xFFFFFFFFu;
            if (tbl->tableInfo.elems) {
                auto* elems = static_cast<uint32_t*>(tbl->tableInfo.elems);
                elems[index] = kNullRef;
            }
            return JSValueMakeUndefined(ctx);
        }

        // For non-null values, we can't easily convert a JS function back to a WASM
        // function index. Log a warning.
        logger::warn("[WASM] Table.set: setting non-null values is not yet supported");
        JSStringRef errStr = JSStringCreateWithUTF8CString(
            "Table.set: only null values are currently supported");
        JSValueRef errVal = JSValueMakeString(ctx, errStr);
        JSStringRelease(errStr);
        *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
        return JSValueMakeUndefined(ctx);
    }

    // --- Table.prototype.grow(delta) -> oldLength ---

    static JSValueRef WASM_TableGrow(JSContextRef ctx, JSObjectRef /*function*/,
                                      JSObjectRef thisObject, size_t argc,
                                      const JSValueRef argv[], JSValueRef* exception) {
        auto* tbl = static_cast<WASMTableData*>(JSObjectGetPrivate(thisObject));
        if (!tbl || !tbl->moduleInst) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("Table.grow: invalid table object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        if (argc < 1) {
            JSStringRef errStr = JSStringCreateWithUTF8CString("Table.grow: requires delta argument");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeUndefined(ctx);
        }

        // [063] Validate delta before casting — negative wraps to huge uint32_t, NaN is UB
        double dDelta = JSValueToNumber(ctx, argv[0], nullptr);
        if (std::isnan(dDelta) || dDelta < 0.0 || dDelta > static_cast<double>(UINT32_MAX)) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Table.grow: delta must be a non-negative integer");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeNumber(ctx, -1);
        }
        uint32_t delta = static_cast<uint32_t>(dDelta);
        uint32_t oldSize = tbl->tableInfo.cur_size;

        // Re-fetch current size from WAMR in case WASM-side grow changed it
        if (tbl->ownsTable && tbl->moduleInst) {
            wasm_table_inst_t freshInfo{};
            if (wasm_runtime_get_export_table_inst(tbl->moduleInst, "table", &freshInfo)) {
                oldSize = freshInfo.cur_size;
                tbl->tableInfo.cur_size = freshInfo.cur_size;
                tbl->tableInfo.max_size = freshInfo.max_size;
            }
        }

        // [063] Check for overflow before computing newSize
        if (delta > UINT32_MAX - oldSize) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Table.grow: size overflow");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeNumber(ctx, -1);
        }
        uint32_t newSize = oldSize + delta;

        if (tbl->tableInfo.max_size != UINT32_MAX && newSize > tbl->tableInfo.max_size) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Table.grow: would exceed maximum table size");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeNumber(ctx, -1);
        }

        // Call WAMR's internal table enlarge function.
        // NULL_REF (0xFFFFFFFF) initializes new entries to null references.
        constexpr uintptr_t kNullRef = 0xFFFFFFFF;
        auto* internalInst = reinterpret_cast<WASMModuleInstance*>(tbl->moduleInst);
        if (!wasm_enlarge_table(internalInst, tbl->tableIndex, delta, kNullRef)) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Table.grow: WAMR failed to enlarge table");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSValueMakeNumber(ctx, -1);
        }

        // Update cached info after successful grow
        tbl->tableInfo.cur_size = newSize;

        logger::info("[WASM] Table.grow: {} -> {} entries", oldSize, newSize);
        return JSValueMakeNumber(ctx, static_cast<double>(oldSize));
    }

    // --- GetWASMTableClass ---

    JSClassRef GetWASMTableClass() {
        static JSClassRef cls = []() {
            static JSStaticValue kTableValues[] = {
                {"length", WASM_TableGetLength, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
                {nullptr, nullptr, nullptr, 0}
            };

            static JSStaticFunction kTableFunctions[] = {
                {"get", WASM_TableGet, kJSPropertyAttributeDontDelete},
                {"set", WASM_TableSet, kJSPropertyAttributeDontDelete},
                {"grow", WASM_TableGrow, kJSPropertyAttributeDontDelete},
                {nullptr, nullptr, 0}
            };

            JSClassDefinition def{};
            def.className = "WebAssembly.Table";
            def.finalize = WASMTableFinalize;
            def.staticValues = kTableValues;
            def.staticFunctions = kTableFunctions;
            return JSClassCreate(&def);
        }();
        return cls;
    }

    // --- WebAssembly.Table constructor ---

    JSObjectRef WASM_TableConstructor(JSContextRef ctx, JSObjectRef /*constructor*/,
                                      size_t argc, const JSValueRef argv[],
                                      JSValueRef* exception) {
        if (argc < 1 || !JSValueIsObject(ctx, argv[0])) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Table requires a descriptor object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        JSObjectRef descriptor = JSValueToObject(ctx, argv[0], nullptr);

        // Read 'element' (required, must be "anyfunc" or "funcref")
        JSStringRef elementProp = JSStringCreateWithUTF8CString("element");
        JSValueRef elementVal = JSObjectGetProperty(ctx, descriptor, elementProp, nullptr);
        JSStringRelease(elementProp);

        if (!JSValueIsString(ctx, elementVal)) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Table: 'element' is required and must be a string");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        JSStringRef elemStr = JSValueToStringCopy(ctx, elementVal, nullptr);
        size_t bufLen = JSStringGetMaximumUTF8CStringSize(elemStr);
        std::string elemType(bufLen, '\0');
        JSStringGetUTF8CString(elemStr, elemType.data(), bufLen);
        JSStringRelease(elemStr);
        elemType.resize(strlen(elemType.c_str()));

        if (elemType != "anyfunc" && elemType != "funcref") {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Table: 'element' must be 'anyfunc' or 'funcref'");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // Read 'initial' (required)
        JSStringRef initialProp = JSStringCreateWithUTF8CString("initial");
        JSValueRef initialVal = JSObjectGetProperty(ctx, descriptor, initialProp, nullptr);
        JSStringRelease(initialProp);

        if (!JSValueIsNumber(ctx, initialVal)) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Table: 'initial' is required and must be a number");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        uint32_t initial = static_cast<uint32_t>(JSValueToNumber(ctx, initialVal, nullptr));

        // Read 'maximum' (optional)
        uint32_t maximum = UINT32_MAX;
        JSStringRef maximumProp = JSStringCreateWithUTF8CString("maximum");
        JSValueRef maximumVal = JSObjectGetProperty(ctx, descriptor, maximumProp, nullptr);
        JSStringRelease(maximumProp);

        if (JSValueIsNumber(ctx, maximumVal)) {
            maximum = static_cast<uint32_t>(JSValueToNumber(ctx, maximumVal, nullptr));
        }

        if (initial > maximum) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Table: 'initial' must not exceed 'maximum'");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        if (!EnsureRuntimeInitialized()) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Table: failed to initialize WASM runtime");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // Build a minimal WASM module with a table export, similar to how
        // standalone Memory is created.
        std::string tblName = "table";
        std::vector<uint8_t> wasmBytes;

        auto emitLEB128 = [&wasmBytes](uint32_t val) {
            do {
                uint8_t byte = val & 0x7F;
                val >>= 7;
                if (val != 0) byte |= 0x80;
                wasmBytes.push_back(byte);
            } while (val != 0);
        };

        // Magic + version
        wasmBytes.insert(wasmBytes.end(), {0x00, 0x61, 0x73, 0x6D});
        wasmBytes.insert(wasmBytes.end(), {0x01, 0x00, 0x00, 0x00});

        // Table section (section id = 4)
        {
            std::vector<uint8_t> tblSection;
            auto emitLEB = [&tblSection](uint32_t val) {
                do {
                    uint8_t byte = val & 0x7F;
                    val >>= 7;
                    if (val != 0) byte |= 0x80;
                    tblSection.push_back(byte);
                } while (val != 0);
            };
            emitLEB(1);                    // count: 1 table
            tblSection.push_back(0x70);    // element type: funcref
            if (maximum != UINT32_MAX) {
                tblSection.push_back(0x01);  // flags: has_max
                emitLEB(initial);
                emitLEB(maximum);
            } else {
                tblSection.push_back(0x00);  // flags: no max
                emitLEB(initial);
            }

            wasmBytes.push_back(0x04);  // section id: table
            emitLEB128(static_cast<uint32_t>(tblSection.size()));
            wasmBytes.insert(wasmBytes.end(), tblSection.begin(), tblSection.end());
        }

        // Export section (section id = 7)
        {
            std::vector<uint8_t> expSection;
            auto emitLEB = [&expSection](uint32_t val) {
                do {
                    uint8_t byte = val & 0x7F;
                    val >>= 7;
                    if (val != 0) byte |= 0x80;
                    expSection.push_back(byte);
                } while (val != 0);
            };
            emitLEB(1);  // count: 1 export
            emitLEB(static_cast<uint32_t>(tblName.size()));
            expSection.insert(expSection.end(), tblName.begin(), tblName.end());
            expSection.push_back(0x01);  // kind: table
            emitLEB(0);                  // index: 0

            wasmBytes.push_back(0x07);  // section id: export
            emitLEB128(static_cast<uint32_t>(expSection.size()));
            wasmBytes.insert(wasmBytes.end(), expSection.begin(), expSection.end());
        }

        char errorBuf[128] = {};
        wasm_module_t miniModule = wasm_runtime_load(
            wasmBytes.data(), static_cast<uint32_t>(wasmBytes.size()),
            errorBuf, sizeof(errorBuf));

        if (!miniModule) {
            std::string errMsg = "WebAssembly.Table: internal module load failed: ";
            errMsg += errorBuf;
            logger::error("[WASM] {}", errMsg);

            JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        wasm_module_inst_t miniInst = wasm_runtime_instantiate(
            miniModule, 4096, 0, errorBuf, sizeof(errorBuf));

        if (!miniInst) {
            std::string errMsg = "WebAssembly.Table: internal instantiation failed: ";
            errMsg += errorBuf;
            logger::error("[WASM] {}", errMsg);
            wasm_runtime_unload(miniModule);

            JSStringRef errStr = JSStringCreateWithUTF8CString(errMsg.c_str());
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        wasm_table_inst_t tableInfo{};
        if (!wasm_runtime_get_export_table_inst(miniInst, "table", &tableInfo)) {
            logger::error("[WASM] WebAssembly.Table: no table after instantiation");
            wasm_runtime_deinstantiate(miniInst);
            wasm_runtime_unload(miniModule);

            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Table: failed to create table");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // [061] Store ctx for finalizer use and miniModule for cleanup
        auto* tblData = new WASMTableData{miniInst, tableInfo, 0, nullptr, true, ctx, nullptr, miniModule};

        AddLiveObject();
        JSObjectRef tblObj = JSObjectMake(ctx, GetWASMTableClass(), tblData);

        logger::info("[WASM] Table created: initial={}, maximum={} entries", initial,
                     maximum == UINT32_MAX ? 0 : maximum);
        return tblObj;
    }

    // --- WrapTableExport ---

    JSObjectRef WrapTableExport(JSContextRef ctx, wasm_module_inst_t moduleInst,
                                const wasm_table_inst_t& tableInfo, uint32_t tableIndex,
                                wasm_exec_env_t execEnv,
                                JSObjectRef instanceObj) {
        auto* tblData = new WASMTableData{moduleInst, tableInfo, tableIndex, execEnv, false,
                                          ctx, instanceObj};
        if (instanceObj) {
            JSValueProtect(ctx, instanceObj);
        }

        AddLiveObject();
        return JSObjectMake(ctx, GetWASMTableClass(), tblData);
    }

    // =========================================================================
    //  GLOBAL
    // =========================================================================

    // --- Global finalizer ---

    static void WASMGlobalFinalize(JSObjectRef obj) {
        auto* data = static_cast<WASMGlobalData*>(JSObjectGetPrivate(obj));
        if (data) {
            delete data;
            RemoveLiveObject();
        }
    }

    // --- Helper: read global value as JSValue ---

    static JSValueRef ReadGlobalValue(JSContextRef ctx, const WASMGlobalData* g) {
        if (!g->globalInfo.global_data) {
            return JSValueMakeNumber(ctx, 0);
        }

        switch (g->globalInfo.kind) {
            case WASM_I32: {
                int32_t val;
                memcpy(&val, g->globalInfo.global_data, sizeof(int32_t));
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
            case WASM_I64: {
                int64_t val;
                memcpy(&val, g->globalInfo.global_data, sizeof(int64_t));
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
            case WASM_F32: {
                float val;
                memcpy(&val, g->globalInfo.global_data, sizeof(float));
                return JSValueMakeNumber(ctx, static_cast<double>(val));
            }
            case WASM_F64: {
                double val;
                memcpy(&val, g->globalInfo.global_data, sizeof(double));
                return JSValueMakeNumber(ctx, val);
            }
            default:
                return JSValueMakeNumber(ctx, 0);
        }
    }

    // --- Helper: write JS value to global ---

    static bool WriteGlobalValue(JSContextRef ctx, WASMGlobalData* g, JSValueRef jsVal) {
        if (!g->globalInfo.global_data) return false;

        double num = JSValueToNumber(ctx, jsVal, nullptr);

        switch (g->globalInfo.kind) {
            case WASM_I32: {
                int32_t val = static_cast<int32_t>(num);
                memcpy(g->globalInfo.global_data, &val, sizeof(int32_t));
                return true;
            }
            case WASM_I64: {
                int64_t val = static_cast<int64_t>(num);
                memcpy(g->globalInfo.global_data, &val, sizeof(int64_t));
                return true;
            }
            case WASM_F32: {
                float val = static_cast<float>(num);
                memcpy(g->globalInfo.global_data, &val, sizeof(float));
                return true;
            }
            case WASM_F64: {
                memcpy(g->globalInfo.global_data, &num, sizeof(double));
                return true;
            }
            default:
                return false;
        }
    }

    // --- Global.prototype.value (getter) ---

    static JSValueRef WASM_GlobalGetValue(JSContextRef ctx, JSObjectRef object,
                                           JSStringRef /*propertyName*/, JSValueRef* /*exception*/) {
        auto* g = static_cast<WASMGlobalData*>(JSObjectGetPrivate(object));
        if (!g) return JSValueMakeUndefined(ctx);
        return ReadGlobalValue(ctx, g);
    }

    // --- Global.prototype.value (setter) ---

    static bool WASM_GlobalSetValue(JSContextRef ctx, JSObjectRef object,
                                     JSStringRef /*propertyName*/, JSValueRef value,
                                     JSValueRef* exception) {
        auto* g = static_cast<WASMGlobalData*>(JSObjectGetPrivate(object));
        if (!g) return false;

        if (!g->globalInfo.is_mutable) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "Global.value: cannot set value of immutable global");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return false;
        }

        return WriteGlobalValue(ctx, g, value);
    }

    // --- GetWASMGlobalClass ---

    JSClassRef GetWASMGlobalClass() {
        static JSClassRef cls = []() {
            static JSStaticValue kGlobalValues[] = {
                {"value", WASM_GlobalGetValue, WASM_GlobalSetValue, kJSPropertyAttributeDontDelete},
                {nullptr, nullptr, nullptr, 0}
            };

            JSClassDefinition def{};
            def.className = "WebAssembly.Global";
            def.finalize = WASMGlobalFinalize;
            def.staticValues = kGlobalValues;
            return JSClassCreate(&def);
        }();
        return cls;
    }

    // --- WebAssembly.Global constructor ---

    JSObjectRef WASM_GlobalConstructor(JSContextRef ctx, JSObjectRef /*constructor*/,
                                       size_t argc, const JSValueRef argv[],
                                       JSValueRef* exception) {
        if (argc < 1 || !JSValueIsObject(ctx, argv[0])) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Global requires a descriptor object");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        JSObjectRef descriptor = JSValueToObject(ctx, argv[0], nullptr);

        // Read 'value' type (required, e.g. "i32", "i64", "f32", "f64")
        JSStringRef valueProp = JSStringCreateWithUTF8CString("value");
        JSValueRef valueTypeVal = JSObjectGetProperty(ctx, descriptor, valueProp, nullptr);
        JSStringRelease(valueProp);

        if (!JSValueIsString(ctx, valueTypeVal)) {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Global: 'value' type is required and must be a string");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        JSStringRef vtStr = JSValueToStringCopy(ctx, valueTypeVal, nullptr);
        size_t bufLen = JSStringGetMaximumUTF8CStringSize(vtStr);
        std::string valueType(bufLen, '\0');
        JSStringGetUTF8CString(vtStr, valueType.data(), bufLen);
        JSStringRelease(vtStr);
        valueType.resize(strlen(valueType.c_str()));

        wasm_valkind_t kind;
        if (valueType == "i32") kind = WASM_I32;
        else if (valueType == "i64") kind = WASM_I64;
        else if (valueType == "f32") kind = WASM_F32;
        else if (valueType == "f64") kind = WASM_F64;
        else {
            JSStringRef errStr = JSStringCreateWithUTF8CString(
                "WebAssembly.Global: 'value' must be 'i32', 'i64', 'f32', or 'f64'");
            JSValueRef errVal = JSValueMakeString(ctx, errStr);
            JSStringRelease(errStr);
            *exception = JSObjectMakeError(ctx, 1, &errVal, nullptr);
            return JSObjectMake(ctx, nullptr, nullptr);
        }

        // Read 'mutable' (optional, default false)
        bool isMutable = false;
        JSStringRef mutableProp = JSStringCreateWithUTF8CString("mutable");
        JSValueRef mutableVal = JSObjectGetProperty(ctx, descriptor, mutableProp, nullptr);
        JSStringRelease(mutableProp);

        if (JSValueIsBoolean(ctx, mutableVal)) {
            isMutable = JSValueToBoolean(ctx, mutableVal);
        }

        // Create standalone global with its own storage
        auto* globalData = new WASMGlobalData{};
        globalData->globalInfo.kind = kind;
        globalData->globalInfo.is_mutable = isMutable;
        globalData->globalInfo.global_data = &globalData->storage;
        globalData->ownsGlobal = true;

        // Initialize with the provided value (argv[1]) or 0
        if (argc >= 2) {
            WriteGlobalValue(ctx, globalData, argv[1]);
        }

        AddLiveObject();
        JSObjectRef globalObj = JSObjectMake(ctx, GetWASMGlobalClass(), globalData);

        logger::info("[WASM] Global created: type={}, mutable={}", valueType, isMutable);
        return globalObj;
    }

    // --- WrapGlobalExport ---

    JSObjectRef WrapGlobalExport(JSContextRef ctx, const wasm_global_inst_t& globalInfo) {
        auto* globalData = new WASMGlobalData{};
        globalData->globalInfo = globalInfo;
        globalData->ownsGlobal = false;

        AddLiveObject();
        return JSObjectMake(ctx, GetWASMGlobalClass(), globalData);
    }

}  // namespace PrismaUI::WASM
