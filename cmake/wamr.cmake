# wamr.cmake
# Fetches and configures WAMR (WebAssembly Micro Runtime) for PrismaUI

include(FetchContent)

FetchContent_Declare(wamr
    GIT_REPOSITORY https://github.com/bytecodealliance/wasm-micro-runtime
    GIT_TAG        WAMR-2.2.0
    GIT_SHALLOW    TRUE
)

# WAMR build options — interpreter only, no JIT, Windows x64
set(WAMR_BUILD_PLATFORM    "windows"   CACHE STRING "" FORCE)
set(WAMR_BUILD_TARGET      "X86_64"    CACHE STRING "" FORCE)
set(WAMR_BUILD_INTERP      1           CACHE BOOL "" FORCE)
set(WAMR_BUILD_FAST_INTERP 1           CACHE BOOL "" FORCE)   # Fast interpreter for better performance
set(WAMR_BUILD_JIT         0           CACHE BOOL "" FORCE)
set(WAMR_BUILD_AOT         0           CACHE BOOL "" FORCE)
set(WAMR_BUILD_LIBC_BUILTIN 1          CACHE BOOL "" FORCE)
set(WAMR_BUILD_LIBC_WASI   0           CACHE BOOL "" FORCE)   # No WASI needed for browser-like env
set(WAMR_BUILD_MULTI_MODULE 0          CACHE BOOL "" FORCE)   # Can enable later
set(WAMR_BUILD_THREAD_MGR  0           CACHE BOOL "" FORCE)   # No threads proposal
set(WAMR_BUILD_BULK_MEMORY 1           CACHE BOOL "" FORCE)   # Required by many toolchains
set(WAMR_BUILD_REF_TYPES   1           CACHE BOOL "" FORCE)   # Required by newer WASM
set(WAMR_BUILD_SIMD        1           CACHE BOOL "" FORCE)   # SIMD support (confirmed not crash cause)
set(WAMR_BUILD_SIMDE       1           CACHE BOOL "" FORCE)   # SIMDe fallback for non-SIMD hosts
# Enable WASM-level call stack dumps for diagnostics
set(WAMR_BUILD_DUMP_CALL_STACK   1     CACHE BOOL "" FORCE)
set(WAMR_BUILD_CUSTOM_NAME_SECTION 1   CACHE BOOL "" FORCE)
# Disable hardware-based bounds checking so WAMR uses software bounds checks.
# With HW bounds check on, WAMR maps 8GB of virtual memory and relies on guard
# pages + a VEH to catch out-of-bounds accesses. However, if a faulting address
# falls outside the 8GB range (e.g., 0xFFFFFFFFFFFFFFFF from a corrupted internal
# pointer), WAMR's VEH silently passes the exception through — leaving no WASM
# trap set. With software bounds checking, out-of-bounds accesses are caught by
# explicit comparisons and produce a clean "out of bounds memory access" trap.
set(WAMR_DISABLE_HW_BOUND_CHECK 1  CACHE BOOL "" FORCE)
set(CMAKE_C_COMPILER_ID "MSVC")  # Force MSVC to avoid WAMR's clang-specific flags
# We don't need the shared library (iwasm.dll), only the static lib (iwasm_static / vmlib.lib)
set(WAMR_BUILD_SHARED   0           CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(wamr)

# Helper function to add WAMR to a target
# The WAMR static library target is "iwasm_static" (OUTPUT_NAME = vmlib)
function(add_wamr_dependencies target_name)
    target_link_libraries(${target_name} PRIVATE iwasm_static)
    target_include_directories(${target_name} PRIVATE
        "${wamr_SOURCE_DIR}/core/iwasm/include"
    )
    # When linking statically, override the MSVC dllimport decoration.
    # WAMR headers default to __declspec(dllimport) unless this is defined.
    target_compile_definitions(${target_name} PRIVATE
        WASM_RUNTIME_API_EXTERN=
    )    
endfunction()
