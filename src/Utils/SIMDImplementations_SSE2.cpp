// This file is compiled with SSE2 instructions (baseline x86_64)
#include "SIMDDispatch.h"

#include <emmintrin.h>  // SSE2
#include <cstring>

namespace PrismaUI::SIMD::SSE2 {

    // Inline helper: SSE2 copy with 2x unrolled loop (32 bytes/iter) to reduce loop overhead.
    static __forceinline void CopyBytes_SSE2(uint8_t* dest, const uint8_t* src, size_t bytes) {
        // 2x unrolled: process 32 bytes per iteration
        while (bytes >= 32) {
            __m128i d1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
            __m128i d2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 16));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dest), d1);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dest + 16), d2);
            src += 32;
            dest += 32;
            bytes -= 32;
        }

        // Handle remaining 16-byte chunk
        if (bytes >= 16) {
            __m128i d = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(dest), d);
            src += 16;
            dest += 16;
            bytes -= 16;
        }

        if (bytes > 0) {
            std::memcpy(dest, src, bytes);
        }
    }

    void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                    uint32_t height) {
        const uint8_t* srcBytes = static_cast<const uint8_t*>(src);
        uint8_t* destBytes = static_cast<uint8_t*>(dest);
        const size_t rowBytes = static_cast<size_t>(width) * 4;  // 4 bytes per BGRA pixel

        if (destPitch == srcPitch && srcPitch == rowBytes) {
            // Contiguous memory - delegate to std::memcpy which is already SSE2-optimized
            std::memcpy(destBytes, srcBytes, static_cast<size_t>(height) * rowBytes);
        } else {
            // Row-by-row copy with SSE2 (avoids per-row memcpy function call overhead)
            for (uint32_t y = 0; y < height; ++y) {
                const uint8_t* srcRow = srcBytes + static_cast<size_t>(y) * srcPitch;
                uint8_t* destRow = destBytes + static_cast<size_t>(y) * destPitch;
                CopyBytes_SSE2(destRow, srcRow, rowBytes);
            }
        }
    }

    // Note: Modern CRT memcpy is heavily optimized with runtime CPU detection.
    // This provides a consistent SSE2 baseline for systems where the CRT may not be optimal.
    void FastMemcpy(void* dest, const void* src, size_t size) {
        CopyBytes_SSE2(static_cast<uint8_t*>(dest), static_cast<const uint8_t*>(src), size);
    }

}  // namespace PrismaUI::SIMD::SSE2
