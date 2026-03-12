// This file is compiled with AVX instructions
#include "SIMDDispatch.h"

#include <cassert>
#include <immintrin.h>  // AVX
#include <cstring>

namespace PrismaUI::SIMD::AVX {

    // Inline helper: AVX copy loop with 32-byte chunks.
    static __forceinline void CopyBytes_AVX(uint8_t* dest, const uint8_t* src, size_t bytes) {
        while (bytes >= 32) {
            __m256i data = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest), data);
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
            // Row-by-row copy with AVX (avoids per-row memcpy function call overhead)
            for (uint32_t y = 0; y < height; ++y) {
                const uint8_t* srcRow = srcBytes + static_cast<size_t>(y) * srcPitch;
                uint8_t* destRow = destBytes + static_cast<size_t>(y) * destPitch;
                CopyBytes_AVX(destRow, srcRow, rowBytes);
            }
        }

        // Clean up AVX state (important for mixing AVX and SSE/x87 code)
        _mm256_zeroupper();
    }

    void FastMemcpy(void* dest, const void* src, size_t size) {
        CopyBytes_AVX(static_cast<uint8_t*>(dest), static_cast<const uint8_t*>(src), size);
        _mm256_zeroupper();
    }

    // AVX-tier swizzle: uses SSSE3 128-bit shuffle (AVX implies SSSE3).
    // _mm256_shuffle_epi8 requires AVX2, so we use 128-bit path here.
    void SwizzleFlipPixels(void* dest, const void* src, uint32_t width, uint32_t height) {
        if (dest == src) {
            logger::error("SwizzleFlipPixels: in-place operation not supported");
            return;
        }
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        auto* srcBytes = static_cast<const uint8_t*>(src);
        auto* dstBytes = static_cast<uint8_t*>(dest);

        const __m128i shuffleMask = _mm_setr_epi8(
            2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);
        const __m128i alphaMask = _mm_set_epi8(
            -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0);

        for (uint32_t row = 0; row < height; row++) {
            const uint8_t* srcRow = srcBytes + static_cast<size_t>(row) * rowBytes;
            uint8_t* dstRow = dstBytes + static_cast<size_t>(height - 1 - row) * rowBytes;

            size_t i = 0;
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

}  // namespace PrismaUI::SIMD::AVX
