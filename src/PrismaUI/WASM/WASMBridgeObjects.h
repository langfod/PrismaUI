#pragma once

#include <JavaScriptCore/JavaScript.h>
#include <wasm_export.h>

#include <cstdint>

namespace PrismaUI::WASM {

    // =========================================================================
    // WebAssembly.Memory private data (stored as JSObject private data)
    // =========================================================================

    struct WASMMemoryData {
        wasm_module_inst_t moduleInst = nullptr;  // Owning module instance (miniInst for standalone)
        wasm_memory_inst_t memoryInst = nullptr;  // WAMR memory handle
        JSObjectRef cachedBuffer = nullptr;       // Current ArrayBuffer (JSValueProtect'd when valid)
        void* cachedBase = nullptr;               // Base pointer when cachedBuffer was created
        size_t cachedSize = 0;                    // Byte size when cachedBuffer was created
        bool ownsMemory = false;                  // True if constructed standalone (not from an export)
        // JSC context — needed for JSValueProtect/Unprotect on cachedBuffer and instanceRef
        JSContextRef ctx = nullptr;
        JSObjectRef instanceRef = nullptr;   // JSValueProtect'd (null for standalone)
        wasm_module_t miniModule = nullptr;  // module to unload in finalizer
    };

    // =========================================================================
    // WebAssembly.Table private data
    // =========================================================================

    struct WASMTableData {
        wasm_module_inst_t moduleInst = nullptr;
        wasm_table_inst_t tableInfo{};      // Copy of table instance info
        uint32_t tableIndex = 0;            // Table index within the module
        wasm_exec_env_t execEnv = nullptr;  // For calling functions retrieved from table
        bool ownsTable = false;             // True if constructed standalone
        // JSC context — needed for JSValueProtect/Unprotect on instanceRef
        JSContextRef ctx = nullptr;
        JSObjectRef instanceRef = nullptr;   // JSValueProtect'd (null for standalone)
        wasm_module_t miniModule = nullptr;  // module to unload in finalizer
    };

    // =========================================================================
    // WebAssembly.Global private data
    // =========================================================================

    struct WASMGlobalData {
        wasm_global_inst_t globalInfo{};  // Copy of global instance info (kind, is_mutable, global_data)
        bool ownsGlobal = false;          // True if constructed standalone
        // Standalone globals need their own storage
        union {
            int32_t i32Val;
            int64_t i64Val;
            float f32Val;
            double f64Val;
        } storage{};
        // JSC context — needed for JSValueProtect/Unprotect on instanceRef
        JSContextRef ctx = nullptr;
        JSObjectRef instanceRef = nullptr;  // JSValueProtect'd (null for standalone)
    };

    // =========================================================================
    // JSC class accessors
    // =========================================================================

    JSClassRef GetWASMMemoryClass();
    JSClassRef GetWASMTableClass();
    JSClassRef GetWASMGlobalClass();

    // =========================================================================
    // Constructors
    // =========================================================================

    // JSC constructor: new WebAssembly.Memory({initial, maximum, shared})
    JSObjectRef WASM_MemoryConstructor(JSContextRef ctx, JSObjectRef constructor, size_t argc, const JSValueRef argv[],
                                       JSValueRef* exception);

    // JSC constructor: new WebAssembly.Table({element, initial, maximum})
    JSObjectRef WASM_TableConstructor(JSContextRef ctx, JSObjectRef constructor, size_t argc, const JSValueRef argv[],
                                      JSValueRef* exception);

    // JSC constructor: new WebAssembly.Global({value, mutable}, initValue)
    JSObjectRef WASM_GlobalConstructor(JSContextRef ctx, JSObjectRef constructor, size_t argc, const JSValueRef argv[],
                                       JSValueRef* exception);

    // =========================================================================
    // Helpers for creating wrapper objects from module exports
    // =========================================================================

    JSObjectRef WrapMemoryExport(JSContextRef ctx, wasm_module_inst_t moduleInst, wasm_memory_inst_t memoryInst,
                                 JSObjectRef instanceObj);

    JSObjectRef WrapTableExport(JSContextRef ctx, wasm_module_inst_t moduleInst, const wasm_table_inst_t& tableInfo,
                                uint32_t tableIndex, wasm_exec_env_t execEnv, JSObjectRef instanceObj);

    JSObjectRef WrapGlobalExport(JSContextRef ctx, const wasm_global_inst_t& globalInfo, JSObjectRef instanceObj);

}  // namespace PrismaUI::WASM
