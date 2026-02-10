# SIMD Runtime Dispatch

## Overview

PrismaUI uses **runtime CPU dispatching** to provide optimal performance across a wide range of hardware, from older CPUs without AVX support to modern processors with AVX2 instructions.

Instead of compiling the entire codebase with a specific SIMD instruction set (which would break on older CPUs), we:

1. **Compile critical functions multiple times** with different SIMD flags (SSE2, AVX, AVX2)
2. **Detect CPU capabilities at runtime** using the cpuinfo library
3. **Select the best implementation** through function pointers

This ensures compatibility with all hardware while maximizing performance on capable systems.

## Architecture

### SIMD Instruction Set Levels

| Level | Year | Description | Compile Flag |
|-------|------|-------------|--------------|
| **Generic** | - | Standard C++ (no SIMD) | None |
| **SSE2** | 2001 | 128-bit vectors (baseline x86_64) | `/arch:SSE2` |
| **AVX** | 2011 | 256-bit vectors | `/arch:AVX` |
| **AVX2** | 2013 | 256-bit vectors + integer ops | `/arch:AVX2` |

### File Organization

```
src/Utils/
├── SIMDDispatch.h              # Public API & function pointer declarations
├── SIMDDispatch.cpp            # Initialization & generic implementations
├── SIMDImplementations_SSE2.cpp   # SSE2 optimized versions
├── SIMDImplementations_AVX.cpp    # AVX optimized versions
└── SIMDImplementations_AVX2.cpp   # AVX2 optimized versions
```

Each `SIMDImplementations_*.cpp` file is compiled with its corresponding `/arch:` flag, generating optimized machine code for that instruction set.

## Usage

### Initialization

Call once at startup before using any SIMD functions:

```cpp
#include "Utils/SIMDDispatch.h"

// In main.cpp or initialization code
PrismaUI::SIMD::Initialize();

// Log the detected instruction set
logger::info("Using {} instruction set", 
    PrismaUI::SIMD::GetInstructionSetName(
        PrismaUI::SIMD::GetActiveInstructionSet()
    ));
```

### Available Functions

#### CopyPixels - Optimized Texture Copying

```cpp
// Copy pixel data with optimal SIMD (handles misaligned data & varying pitches)
SIMD::CopyPixels(
    mappedResource.pData,      // destination
    mappedResource.RowPitch,    // destination pitch (bytes per row)
    pixels,                     // source
    stride,                     // source pitch
    width,                      // image width in pixels
    height                      // image height in rows
);
```

**Performance gain**: ~2-3x faster than row-by-row memcpy on AVX2 systems.

#### FastMemcpy - Large Buffer Copying

```cpp
// Fast memcpy for large contiguous buffers
SIMD::FastMemcpy(dest, src, sizeInBytes);
```

**Performance gain**: ~1.5-2x faster than standard memcpy for large buffers.

## Adding New SIMD Functions

To add a new SIMD-optimized function:

### 1. Declare in SIMDDispatch.h

```cpp
// Function signature type
using MyFunctionFunc = void (*)(int* data, size_t count);

// Export function pointer
extern MyFunctionFunc MyFunction;
```

### 2. Implement in each variant file

**Generic version** (`SIMDDispatch.cpp`):
```cpp
namespace Generic {
    void MyFunction(int* data, size_t count) {
        // Standard C++ implementation
    }
}
```

**SSE2 version** (`SIMDImplementations_SSE2.cpp`):
```cpp
#include <emmintrin.h>  // SSE2

namespace SSE2 {
    void MyFunction(int* data, size_t count) {
        // SSE2 implementation using __m128i
    }
}
```

**AVX version** (`SIMDImplementations_AVX.cpp`):
```cpp
#include <immintrin.h>  // AVX

namespace AVX {
    void MyFunction(int* data, size_t count) {
        // AVX implementation using __m256i
        // Remember _mm256_zeroupper() at end!
    }
}
```

**AVX2 version** (`SIMDImplementations_AVX2.cpp`):
```cpp
#include <immintrin.h>  // AVX2

namespace AVX2 {
    void MyFunction(int* data, size_t count) {
        // AVX2 implementation using __m256i
        // Remember _mm256_zeroupper() at end!
    }
}
```

### 3. Initialize function pointer

In `SIMDDispatch.cpp` initialization:

```cpp
// Declare function pointer
MyFunctionFunc MyFunction = Generic::MyFunction;

// In Initialize()
if (cpuinfo_has_x86_avx2()) {
    MyFunction = AVX2::MyFunction;
} else if (cpuinfo_has_x86_avx()) {
    MyFunction = AVX::MyFunction;
} else if (cpuinfo_has_x86_sse2()) {
    MyFunction = SSE2::MyFunction;
}
```

## Good Candidates for SIMD Optimization

✅ **High value targets:**
- Pixel/image processing (copying, blending, color conversion)
- Matrix operations (transforms, projections)
- Vector math (dot products, cross products, normalization)
- Audio processing (mixing, filtering)
- String operations (search, compare) on large data
- Bulk data conversion

❌ **Not worth optimizing:**
- Infrequent operations (once-per-frame or less)
- Small data sets (< 1KB)
- Code with complex branching logic
- Operations dominated by memory latency

## Implementation Guidelines

### When writing SIMD code:

1. **Always provide a generic fallback** - Not all platforms support all instructions
2. **Handle alignment** - Use `_mm_loadu_*` / `_mm_storeu_*` for unaligned data
3. **Process remainder elements** - Handle data that doesn't fit in SIMD chunks
4. **Call `_mm256_zeroupper()`** - Required at end of AVX/AVX2 functions to avoid performance penalties
5. **Avoid function calls in hot loops** - Prevents vectorization
6. **Test edge cases** - Zero-length inputs, misaligned data, etc.

### Example pattern:

```cpp
void ProcessData_AVX2(float* dest, const float* src, size_t count) {
    size_t i = 0;
    
    // Process 8 floats at a time (256 bits / 32 bits per float)
    for (; i + 8 <= count; i += 8) {
        __m256 data = _mm256_loadu_ps(&src[i]);
        // ... SIMD operations ...
        _mm256_storeu_ps(&dest[i], result);
    }
    
    // Process remaining elements
    for (; i < count; ++i) {
        dest[i] = /* scalar operation */ src[i];
    }
    
    _mm256_zeroupper();  // Important!
}
```

## Performance Notes

- **SSE2**: 16 bytes/cycle (baseline, available on all x86_64)
- **AVX**: 32 bytes/cycle (~2x SSE2 for memory operations)
- **AVX2**: 32 bytes/cycle + better integer operations

Real-world gains depend on:
- Memory bandwidth (often the bottleneck)
- Cache behavior
- Data alignment
- Workload characteristics

Always profile before and after optimization to confirm improvements!

## References

- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
- [cpuinfo library](https://github.com/pytorch/cpuinfo)
- [SIMD for C++ Developers](https://www.intel.com/content/www/us/en/developer/articles/technical/a-guide-to-auto-vectorization-with-intel-c-compilers.html)
