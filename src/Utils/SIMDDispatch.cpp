#include "SIMDDispatch.h"

#include <cassert>
#include <atomic>
#include <cpuinfo.h>
#include <cstring>

namespace PrismaUI::SIMD {

    static InstructionSet g_ActiveInstructionSet = InstructionSet::None;
    // [095] Guard: set to true at the end of Initialize(). DEBUG asserts in each
    // Generic:: dispatcher catch callers that skip Initialize() entirely.
    static std::atomic<bool> g_simdInitialized{false};

    // ============================================================
    // Forward declarations of implementation variants
    // ============================================================

    // Generic C++ implementations (compiled without special flags)
    namespace Generic {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height);
        void FastMemcpy(void* dest, const void* src, size_t size);
        void SwizzleFlipPixels(void* dest, const void* src, uint32_t width, uint32_t height);
    }

    // SSE2 implementations (baseline x86_64, always available)
    // SwizzleFlipPixels uses SSSE3 (_mm_shuffle_epi8) — selected only when SSSE3 is detected.
    namespace SSE2 {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height);
        void FastMemcpy(void* dest, const void* src, size_t size);
        void SwizzleFlipPixels(void* dest, const void* src, uint32_t width, uint32_t height);
    }

    // AVX implementations (compiled with /arch:AVX)
    namespace AVX {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height);
        void FastMemcpy(void* dest, const void* src, size_t size);
        void SwizzleFlipPixels(void* dest, const void* src, uint32_t width, uint32_t height);
    }

    // AVX2 implementations (compiled with /arch:AVX2)
    namespace AVX2 {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height);
        void FastMemcpy(void* dest, const void* src, size_t size);
        void SwizzleFlipPixels(void* dest, const void* src, uint32_t width, uint32_t height);
    }

    // ============================================================
    // Function pointers (initialized at runtime)
    // ============================================================

    CopyPixelsFunc CopyPixels = Generic::CopyPixels;
    FastMemcpyFunc FastMemcpy = Generic::FastMemcpy;
    SwizzleFlipPixelsFunc SwizzleFlipPixels = Generic::SwizzleFlipPixels;

    // ============================================================
    // Initialization
    // ============================================================

    void Initialize() {
        // Initialize cpuinfo library
        if (!cpuinfo_initialize()) {
            logger::warn("SIMDDispatch: cpuinfo_initialize() failed, using generic implementations");
            g_ActiveInstructionSet = InstructionSet::None;
            CopyPixels = Generic::CopyPixels;
            FastMemcpy = Generic::FastMemcpy;
            SwizzleFlipPixels = Generic::SwizzleFlipPixels;
            g_simdInitialized.store(true, std::memory_order_release);
            return;
        }

        // Detect CPU capabilities and select best implementation
        // Note: All x86_64 CPUs have SSE2, but we check anyway for safety
        if (cpuinfo_has_x86_avx2()) {
            g_ActiveInstructionSet = InstructionSet::AVX2;
            CopyPixels = AVX2::CopyPixels;
            FastMemcpy = AVX2::FastMemcpy;
            SwizzleFlipPixels = AVX2::SwizzleFlipPixels;
            logger::info("SIMDDispatch: Using AVX2 implementations");
        } else if (cpuinfo_has_x86_avx()) {
            g_ActiveInstructionSet = InstructionSet::AVX;
            CopyPixels = AVX::CopyPixels;
            FastMemcpy = AVX::FastMemcpy;
            // AVX implies SSSE3; use the AVX-optimized swizzle
            SwizzleFlipPixels = AVX::SwizzleFlipPixels;
            logger::info("SIMDDispatch: Using AVX implementations");
        } else if (cpuinfo_has_x86_sse2()) {
            g_ActiveInstructionSet = InstructionSet::SSE2;
            CopyPixels = SSE2::CopyPixels;
            FastMemcpy = SSE2::FastMemcpy;
            // SwizzleFlipPixels uses SSSE3; only select if available
            SwizzleFlipPixels = cpuinfo_has_x86_ssse3()
                ? SSE2::SwizzleFlipPixels
                : Generic::SwizzleFlipPixels;
            logger::info("SIMDDispatch: Using SSE2 implementations");
        } else {
            g_ActiveInstructionSet = InstructionSet::None;
            CopyPixels = Generic::CopyPixels;
            FastMemcpy = Generic::FastMemcpy;
            SwizzleFlipPixels = Generic::SwizzleFlipPixels;
            logger::warn("SIMDDispatch: No SIMD support detected, using generic implementations");
        }

        g_simdInitialized.store(true, std::memory_order_release);
    }

    InstructionSet GetActiveInstructionSet() { return g_ActiveInstructionSet; }

    const char* GetInstructionSetName(InstructionSet set) {
        switch (set) {
            case InstructionSet::None:
                return "Generic";
            case InstructionSet::SSE2:
                return "SSE2";
            case InstructionSet::AVX:
                return "AVX";
            case InstructionSet::AVX2:
                return "AVX2";
            default:
                return "Unknown";
        }
    }

    // ============================================================
    // Generic implementations (no SIMD)
    // ============================================================

    namespace Generic {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height) {
#ifdef _DEBUG
            assert(g_simdInitialized.load(std::memory_order_acquire) &&
                   "SIMD::Initialize() must be called before using SIMD functions");
            // Verify alignment in debug builds (helps catch issues early)
            const uintptr_t destAddr = reinterpret_cast<uintptr_t>(dest);
            const uintptr_t srcAddr = reinterpret_cast<uintptr_t>(src);
            if (destAddr % 32 != 0 || srcAddr % 32 != 0) {
                logger::warn("SIMDDispatch::CopyPixels - Buffer not 32-byte aligned (dest={:#x}, src={:#x}). "
                             "Performance may be reduced.",
                             destAddr, srcAddr);
            }
#endif

            const uint8_t* srcBytes = static_cast<const uint8_t*>(src);
            uint8_t* destBytes = static_cast<uint8_t*>(dest);
            const uint32_t rowBytes = width * 4;  // 4 bytes per BGRA pixel

            if (destPitch == srcPitch && srcPitch == rowBytes) {
                // Contiguous memory - single copy
                std::memcpy(destBytes, srcBytes, static_cast<size_t>(height) * rowBytes);
            } else {
                // Row-by-row copy
                for (uint32_t y = 0; y < height; ++y) {
                    std::memcpy(destBytes + y * destPitch, srcBytes + y * srcPitch, rowBytes);
                }
            }
        }

        void FastMemcpy(void* dest, const void* src, size_t size) {
#ifdef _DEBUG
            assert(g_simdInitialized.load(std::memory_order_acquire) &&
                   "SIMD::Initialize() must be called before using SIMD functions");
            // Verify alignment in debug builds
            const uintptr_t destAddr = reinterpret_cast<uintptr_t>(dest);
            const uintptr_t srcAddr = reinterpret_cast<uintptr_t>(src);
            if (destAddr % 32 != 0 || srcAddr % 32 != 0) {
                logger::warn("SIMDDispatch::FastMemcpy - Buffer not 32-byte aligned (dest={:#x}, src={:#x}). "
                             "Performance may be reduced.",
                             destAddr, srcAddr);
            }
#endif
            std::memcpy(dest, src, size);
        }

        void SwizzleFlipPixels(void* dest, const void* src, uint32_t width, uint32_t height) {
            // [096] In-place operation corrupts data (top rows overwritten before read).
            assert(dest != src && "SwizzleFlipPixels does not support in-place operation");
#ifdef _DEBUG
            assert(g_simdInitialized.load(std::memory_order_acquire) &&
                   "SIMD::Initialize() must be called before using SIMD functions");
#endif
            const size_t rowBytes = static_cast<size_t>(width) * 4;
            auto* srcBytes = static_cast<const uint8_t*>(src);
            auto* dstBytes = static_cast<uint8_t*>(dest);
            for (uint32_t row = 0; row < height; row++) {
                const uint8_t* srcRow = srcBytes + static_cast<size_t>(row) * rowBytes;
                uint8_t* dstRow = dstBytes + static_cast<size_t>(height - 1 - row) * rowBytes;
                for (size_t i = 0; i < rowBytes; i += 4) {
                    dstRow[i + 0] = srcRow[i + 2]; // B <- R
                    dstRow[i + 1] = srcRow[i + 1]; // G
                    dstRow[i + 2] = srcRow[i + 0]; // R <- B
                    dstRow[i + 3] = 255;            // Force opaque
                }
            }
        }
    }  // namespace Generic

}  // namespace PrismaUI::SIMD
