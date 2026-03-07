// This file is compiled with SSE2 instructions (baseline x86_64)
#include "SIMDDispatch.h"

#include <emmintrin.h>  // SSE2
#include <tmmintrin.h>  // SSSE3 for _mm_shuffle_epi8 (used by SwizzleFlipPixels)
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

    // SSSE3 RGBA->BGRA swizzle + vertical flip.
    // Runtime dispatch only selects this when SSSE3 is confirmed available.
    void SwizzleFlipPixels(void* dest, const void* src, uint32_t width, uint32_t height) {
        const uint32_t rowBytes = width * 4;
        auto* srcBytes = static_cast<const uint8_t*>(src);
        auto* dstBytes = static_cast<uint8_t*>(dest);

        // SSSE3 shuffle mask: RGBA -> BGRA for 4 pixels at once
        const __m128i shuffleMask = _mm_setr_epi8(
            2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);
        // Alpha mask: 0xFF at byte positions 3,7,11,15
        const __m128i alphaMask = _mm_set_epi8(
            -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0);

        for (uint32_t row = 0; row < height; row++) {
            const uint8_t* srcRow = srcBytes + row * rowBytes;
            uint8_t* dstRow = dstBytes + (height - 1 - row) * rowBytes;

            uint32_t i = 0;
            for (; i + 16 <= rowBytes; i += 16) {
                __m128i px = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcRow + i));
                px = _mm_shuffle_epi8(px, shuffleMask);
                px = _mm_or_si128(px, alphaMask);
                _mm_storeu_si128(reinterpret_cast<__m128i*>(dstRow + i), px);
            }
            for (; i < rowBytes; i += 4) {
                dstRow[i + 0] = srcRow[i + 2];
                dstRow[i + 1] = srcRow[i + 1];
                dstRow[i + 2] = srcRow[i + 0];
                dstRow[i + 3] = 255;
            }
        }
    }

}  // namespace PrismaUI::SIMD::SSE2
