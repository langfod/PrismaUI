# DirectXMath & Memory Alignment Optimization Guide

## Current State Analysis

### ✅ Good Current Usage
- [GPUTypes.h](d:\proj\PrismaUI\src\PrismaUI\GPU\GPUTypes.h) uses `XMMATRIX` for transforms
- Constant buffer uses DirectXMath types
- Matrix conversions in GPUDriverD3D11

### ⚠️ Opportunities for Improvement

## 1. Memory Alignment for SIMD

### Alignment Requirements by Instruction Set

| Instruction Set | Register Size | Optimal Alignment | Minimum Alignment |
|----------------|---------------|-------------------|-------------------|
| **SSE2** | 128-bit (16 bytes) | 16-byte | 16-byte |
| **AVX** | 256-bit (32 bytes) | 32-byte* | 16-byte |
| **AVX2** | 256-bit (32 bytes) | 32-byte | 16-byte |

**Important:** 32-byte alignment is **recommended not required** for AVX/AVX2. They can work with 16-byte alignment, but:
- ✅ **32-byte aligned**: Single load/store operation
- ⚠️ **16-byte aligned**: May require two operations (performance penalty)
- ❌ **Unaligned**: Performance penalty on all SIMD levels

**Key Insight**: 32-byte alignment helps AVX/AVX2 but does NOT hurt SSE2. SSE2 will simply use the data with its 16-byte alignment requirements satisfied.

### Current Issues

#### Issue #1: Pixel Buffers Not Aligned

**Current code** ([Core.h](d:\proj\PrismaUI\src\PrismaUI\Core.h#L95)):
```cpp
struct PrismaView {
    std::vector<std::byte> pixelBuffer;           // Default 8-byte alignment
    std::vector<std::byte> inspectorPixelBuffer;  // Default 8-byte alignment
};
```

**Problem**: `std::vector` uses default allocator with only 8-byte alignment guarantee. This causes:
- Unaligned loads/stores in SIMD code (performance penalty)
- Potential crashes on some SIMD instructions that require alignment

**Solution**: Use aligned allocator

```cpp
// Define aligned allocator
template<typename T, size_t Alignment>
struct AlignedAllocator {
    using value_type = T;
    
    AlignedAllocator() = default;
    
    template<typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}
    
    T* allocate(size_t n) {
        void* ptr = _aligned_malloc(n * sizeof(T), Alignment);
        if (!ptr) throw std::bad_alloc();
        return static_cast<T*>(ptr);
    }
    
    void deallocate(T* ptr, size_t) noexcept {
        _aligned_free(ptr);
    }
};

template<typename T, typename U, size_t Alignment>
bool operator==(const AlignedAllocator<T, Alignment>&, 
                const AlignedAllocator<U, Alignment>&) { return true; }

template<typename T, typename U, size_t Alignment>
bool operator!=(const AlignedAllocator<T, Alignment>&, 
                const AlignedAllocator<U, Alignment>&) { return false; }

// In PrismaView struct:
struct PrismaView {
    // 32-byte aligned buffers for optimal AVX2 performance
    // Also works fine with SSE2 (which only needs 16-byte)
    std::vector<std::byte, AlignedAllocator<std::byte, 32>> pixelBuffer;
    std::vector<std::byte, AlignedAllocator<std::byte, 32>> inspectorPixelBuffer;
};
```

**Benefits:**
- ✅ SSE2: Guaranteed 16-byte alignment (first 16 bytes of 32-byte block)
- ✅ AVX: Can use 32-byte alignment if available
- ✅ AVX2: Optimal 32-byte alignment
- ✅ No code changes needed in SIMD implementations
- ✅ Automatic cleanup via RAII

#### Issue #2: Constant Buffer Alignment

**Current code** ([GPUTypes.h](d:\proj\PrismaUI\src\PrismaUI\GPU\GPUTypes.h#L33)):
```cpp
struct CB_UltralightData {
    DirectX::XMFLOAT4 State;
    DirectX::XMMATRIX Transform;      // Requires 16-byte alignment
    DirectX::XMINT4   Integer4[2];
    DirectX::XMFLOAT4 Scalar4[2];
    DirectX::XMFLOAT4 Vector[8];
    DirectX::XMINT4   ClipData;
    DirectX::XMMATRIX Clip[8];        // Requires 16-byte alignment
};
```

**Problem**: No explicit alignment guarantee. While DirectXMath types have alignment attributes, it's better to be explicit.

**Solution**:
```cpp
// Ensure proper alignment for DirectXMath types
struct alignas(16) CB_UltralightData {
    DirectX::XMFLOAT4 State;
    DirectX::XMMATRIX Transform;
    DirectX::XMINT4   Integer4[2];
    DirectX::XMFLOAT4 Scalar4[2];
    DirectX::XMFLOAT4 Vector[8];
    DirectX::XMINT4   ClipData;
    DirectX::XMMATRIX Clip[8];
};
```

**Why 16-byte not 32-byte?**
- XMMATRIX requires 16-byte alignment (enforced by DirectXMath)
- Constant buffers must be 16-byte aligned for D3D11
- 32-byte would be wasteful here and unnecessary

## 2. Leverage XMVECTOR & XMMATRIX More Extensively

### Opportunity #1: Vector Conversion Loop

**Current code** ([GPUDriverD3D11.cpp](d:\proj\PrismaUI\src\PrismaUI\GPU\GPUDriverD3D11.cpp#L510)):
```cpp
for (size_t i = 0; i < 8; ++i)
    cbdata.Vector[i] = DirectX::XMFLOAT4(
        state.uniform_vector[i].x, 
        state.uniform_vector[i].y,
        state.uniform_vector[i].z, 
        state.uniform_vector[i].w
    );
```

**Improved version using XMVECTOR:**
```cpp
// Load as XMVECTOR, then store as XMFLOAT4 (lets compiler optimize)
for (size_t i = 0; i < 8; ++i) {
    DirectX::XMVECTOR vec = DirectX::XMVectorSet(
        state.uniform_vector[i].x,
        state.uniform_vector[i].y,
        state.uniform_vector[i].z,
        state.uniform_vector[i].w
    );
    DirectX::XMStoreFloat4(&cbdata.Vector[i], vec);
}
```

**Why this helps:**
- `XMVectorSet` + `XMStoreFloat4` allows compiler to use SIMD instructions
- Original code does component-wise assignments (scalar operations)
- Small gain but adds up when called frequently

### Opportunity #2: Matrix Operations

**Current code** ([GPUDriverD3D11.cpp](d:\proj\PrismaUI\src\PrismaUI\GPU\GPUDriverD3D11.cpp#L545)):
```cpp
ultralight::Matrix GPUDriverD3D11::ApplyProjection(
    const ultralight::Matrix4x4& transform, 
    float screenWidth,
    float screenHeight
) {
    ultralight::Matrix transformMatrix;
    transformMatrix.Set(transform);

    ultralight::Matrix result;
    result.SetOrthographicProjection(screenWidth, screenHeight, false);
    result.Transform(transformMatrix);  // Matrix multiplication

    return result;
}
```

**This is already pretty good** because Ultralight's Matrix class likely uses SIMD internally. However, if you needed to do custom matrix operations:

```cpp
// Example: Custom matrix multiply using DirectXMath
DirectX::XMMATRIX MultiplyMatrices(const DirectX::XMMATRIX& a, const DirectX::XMMATRIX& b) {
    // DirectXMath automatically uses SIMD (SSE2/AVX/AVX2)
    return DirectX::XMMatrixMultiply(a, b);
}

// Example: Transform multiple points at once
void TransformPoints(DirectX::XMFLOAT3* points, size_t count, const DirectX::XMMATRIX& transform) {
    for (size_t i = 0; i < count; ++i) {
        DirectX::XMVECTOR vec = DirectX::XMLoadFloat3(&points[i]);
        vec = DirectX::XMVector3Transform(vec, transform);
        DirectX::XMStoreFloat3(&points[i], vec);
    }
}
```

### Opportunity #3: Batch Vector Operations

If you ever need to process multiple vectors/colors:

```cpp
// Example: Batch normalize vectors
void NormalizeVectors(DirectX::XMFLOAT3* vectors, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        DirectX::XMVECTOR v = DirectX::XMLoadFloat3(&vectors[i]);
        v = DirectX::XMVector3Normalize(v);
        DirectX::XMStoreFloat3(&vectors[i], v);
    }
}

// Example: Batch color transformations (if needed in future)
void ApplyColorTransform(uint32_t* colors, size_t count, float multiplier) {
    DirectX::XMVECTOR mult = DirectX::XMVectorReplicate(multiplier);
    
    for (size_t i = 0; i < count; ++i) {
        // Load BGRA color as vector
        DirectX::XMVECTOR color = DirectX::XMLoadColor((const DirectX::XMCOLOR*)&colors[i]);
        color = DirectX::XMVectorMultiply(color, mult);
        DirectX::XMStoreColor((DirectX::XMCOLOR*)&colors[i], color);
    }
}
```

## 3. Implementation Priority

### High Priority (Do First)

1. **Add aligned allocator for pixel buffers** ⭐⭐⭐
   - Biggest immediate impact
   - Enables full SIMD performance
   - Relatively easy to implement

2. **Add `alignas(16)` to CB_UltralightData** ⭐⭐
   - Ensures correctness
   - Prevents potential issues
   - One-line change

### Medium Priority

3. **Optimize vector conversion loop** ⭐
   - Small but measurable gain
   - Good practice
   - Easy to implement

### Low Priority (Nice to Have)

4. **Add DirectXMath utility functions** 
   - Only if you add features that need them
   - Current code is already decent

## 4. Implementation Code

### Step 1: Create aligned allocator header

```cpp
// File: src/Utils/AlignedAllocator.h
#pragma once
#include <malloc.h>
#include <new>

namespace PrismaUI::Utils {

    template<typename T, size_t Alignment = 32>
    struct AlignedAllocator {
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        template<typename U>
        struct rebind {
            using other = AlignedAllocator<U, Alignment>;
        };

        AlignedAllocator() noexcept = default;

        template<typename U>
        AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

        T* allocate(size_t n) {
            if (n == 0) return nullptr;
            
            size_t size = n * sizeof(T);
            void* ptr = _aligned_malloc(size, Alignment);
            
            if (!ptr) throw std::bad_alloc();
            
            return static_cast<T*>(ptr);
        }

        void deallocate(T* ptr, size_t) noexcept {
            if (ptr) _aligned_free(ptr);
        }

        template<typename U, typename... Args>
        void construct(U* ptr, Args&&... args) {
            ::new((void*)ptr) U(std::forward<Args>(args)...);
        }

        template<typename U>
        void destroy(U* ptr) {
            ptr->~U();
        }
    };

    template<typename T, typename U, size_t Alignment>
    bool operator==(const AlignedAllocator<T, Alignment>&, 
                    const AlignedAllocator<U, Alignment>&) noexcept {
        return true;
    }

    template<typename T, typename U, size_t Alignment>
    bool operator!=(const AlignedAllocator<T, Alignment>&, 
                    const AlignedAllocator<U, Alignment>&) noexcept {
        return false;
    }

}  // namespace PrismaUI::Utils
```

### Step 2: Update Core.h

```cpp
#include "Utils/AlignedAllocator.h"

struct PrismaView {
    // ... existing members ...
    
    // Use 32-byte aligned buffers for optimal SIMD performance
    std::vector<std::byte, Utils::AlignedAllocator<std::byte, 32>> pixelBuffer;
    std::vector<std::byte, Utils::AlignedAllocator<std::byte, 32>> inspectorPixelBuffer;
    
    // ... rest of members ...
};
```

### Step 3: Update GPUTypes.h

```cpp
struct alignas(16) CB_UltralightData {
    DirectX::XMFLOAT4 State;
    DirectX::XMMATRIX Transform;
    DirectX::XMINT4   Integer4[2];
    DirectX::XMFLOAT4 Scalar4[2];
    DirectX::XMFLOAT4 Vector[8];
    DirectX::XMINT4   ClipData;
    DirectX::XMMATRIX Clip[8];
};
```

### Step 4: (Optional) Optimize vector loop

```cpp
// In GPUDriverD3D11::UpdateConstantBuffer
for (size_t i = 0; i < 8; ++i) {
    DirectX::XMVECTOR vec = DirectX::XMVectorSet(
        state.uniform_vector[i].x,
        state.uniform_vector[i].y,
        state.uniform_vector[i].z,
        state.uniform_vector[i].w
    );
    DirectX::XMStoreFloat4(&cbdata.Vector[i], vec);
}
```

## 5. Testing & Validation

### Verify Alignment

Add debug checks:

```cpp
#ifdef _DEBUG
    // Check alignment of pixel buffers
    assert(reinterpret_cast<uintptr_t>(pixelBuffer.data()) % 32 == 0 && 
           "pixelBuffer not 32-byte aligned!");
    assert(reinterpret_cast<uintptr_t>(inspectorPixelBuffer.data()) % 32 == 0 && 
           "inspectorPixelBuffer not 32-byte aligned!");
#endif
```

### Measure Performance

Before and after:
1. Measure frame time for texture uploads
2. Profile with VTune or similar
3. Check if AVX2 instructions are being generated

## Summary

### What to Do

✅ **Must do:**
1. Add aligned allocator for pixel buffers (32-byte)
2. Add `alignas(16)` to constant buffer structure

✅ **Should do:**
3. Optimize vector conversion loop with XMVECTOR

✅ **Nice to have:**
4. Add DirectXMath utility functions as needed

### Alignment Clarification

**32-byte alignment for buffers:**
- ✅ Optimal for AVX2
- ✅ Works fine for AVX
- ✅ Works fine for SSE2 (uses first 16 bytes)
- ✅ No downside for older CPUs

**16-byte alignment for constant buffer:**
- ✅ Required by DirectXMath
- ✅ Required by D3D11
- ✅ Sufficient for all instruction sets

The key insight: **More alignment is generally fine, less alignment can cause problems.** 32-byte alignment satisfies all smaller alignment requirements.
