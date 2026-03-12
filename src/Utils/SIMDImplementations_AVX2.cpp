// This file is compiled with AVX2 instructions
#include "SIMDDispatch.h"

#include <cassert>
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

    void SwizzleFlipPixels(void* dest, const void* src, uint32_t width, uint32_t height) {
        if (dest == src) {
            logger::error("SwizzleFlipPixels: in-place operation not supported");
            return;
        }
        const size_t rowBytes = static_cast<size_t>(width) * 4;
        auto* srcBytes = static_cast<const uint8_t*>(src);
        auto* dstBytes = static_cast<uint8_t*>(dest);

        // AVX2 shuffle operates on two 128-bit lanes independently
        const __m256i shuffleMask = _mm256_setr_epi8(
            2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15,
            2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15);
        const __m256i alphaMask = _mm256_set_epi8(
            -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0,
            -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0);

        for (uint32_t row = 0; row < height; row++) {
            const uint8_t* srcRow = srcBytes + static_cast<size_t>(row) * rowBytes;
            uint8_t* dstRow = dstBytes + static_cast<size_t>(height - 1 - row) * rowBytes;

            size_t i = 0;
            for (; i + 32 <= rowBytes; i += 32) {
                __m256i px = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcRow + i));
                px = _mm256_shuffle_epi8(px, shuffleMask);
                px = _mm256_or_si256(px, alphaMask);
                _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstRow + i), px);
            }
            for (; i < rowBytes; i += 4) {
                dstRow[i + 0] = srcRow[i + 2];
                dstRow[i + 1] = srcRow[i + 1];
                dstRow[i + 2] = srcRow[i + 0];
                dstRow[i + 3] = 255;
            }
        }

        _mm256_zeroupper();
    }

}  // namespace PrismaUI::SIMD::AVX2
