// This file is compiled with AVX2 instructions
#include "SIMDDispatch.h"

#include <immintrin.h>  // AVX2
#include <cstring>

namespace PrismaUI::SIMD::AVX2 {

    // Inline helper: AVX2 copy with 2x 256-bit unrolled loop (64 bytes/iter).
    static __forceinline void CopyBytes_AVX2(uint8_t* dest, const uint8_t* src, size_t bytes) {
        // 2x unrolled: process 64 bytes per iteration
        while (bytes >= 64) {
            __m256i d1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
            __m256i d2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src + 32));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest), d1);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest + 32), d2);
            src += 64;
            dest += 64;
            bytes -= 64;
        }

        // Handle remaining 32-byte chunk
        if (bytes >= 32) {
            __m256i d = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest), d);
            src += 32;
            dest += 32;
            bytes -= 32;
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
            // Contiguous memory - delegate to std::memcpy (already well-optimized on modern CRTs)
            std::memcpy(destBytes, srcBytes, static_cast<size_t>(height) * rowBytes);
        } else {
            // Row-by-row copy with AVX2 (avoids per-row memcpy function call overhead)
            for (uint32_t y = 0; y < height; ++y) {
                const uint8_t* srcRow = srcBytes + static_cast<size_t>(y) * srcPitch;
                uint8_t* destRow = destBytes + static_cast<size_t>(y) * destPitch;
                CopyBytes_AVX2(destRow, srcRow, rowBytes);
            }
        }

        _mm256_zeroupper();
    }

    void FastMemcpy(void* dest, const void* src, size_t size) {
        CopyBytes_AVX2(static_cast<uint8_t*>(dest), static_cast<const uint8_t*>(src), size);
        _mm256_zeroupper();
    }

}  // namespace PrismaUI::SIMD::AVX2
