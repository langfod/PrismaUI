#include "SIMDDispatch.h"

#include <cpuinfo.h>
#include <cstring>

namespace PrismaUI::SIMD {

    static InstructionSet g_ActiveInstructionSet = InstructionSet::None;

    // ============================================================
    // Forward declarations of implementation variants
    // ============================================================

    // Generic C++ implementations (compiled without special flags)
    namespace Generic {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height);
        void FastMemcpy(void* dest, const void* src, size_t size);
    }

    // SSE2 implementations (baseline x86_64, always available)
    namespace SSE2 {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height);
        void FastMemcpy(void* dest, const void* src, size_t size);
    }

    // AVX implementations (compiled with /arch:AVX)
    namespace AVX {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height);
        void FastMemcpy(void* dest, const void* src, size_t size);
    }

    // AVX2 implementations (compiled with /arch:AVX2)
    namespace AVX2 {
        void CopyPixels(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch, uint32_t width,
                        uint32_t height);
        void FastMemcpy(void* dest, const void* src, size_t size);
    }

    // ============================================================
    // Function pointers (initialized at runtime)
    // ============================================================

    CopyPixelsFunc CopyPixels = Generic::CopyPixels;
    FastMemcpyFunc FastMemcpy = Generic::FastMemcpy;

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
            return;
        }

        // Detect CPU capabilities and select best implementation
        // Note: All x86_64 CPUs have SSE2, but we check anyway for safety
        if (cpuinfo_has_x86_avx2()) {
            g_ActiveInstructionSet = InstructionSet::AVX2;
            CopyPixels = AVX2::CopyPixels;
            FastMemcpy = AVX2::FastMemcpy;
            logger::info("SIMDDispatch: Using AVX2 implementations");
        } else if (cpuinfo_has_x86_avx()) {
            g_ActiveInstructionSet = InstructionSet::AVX;
            CopyPixels = AVX::CopyPixels;
            FastMemcpy = AVX::FastMemcpy;
            logger::info("SIMDDispatch: Using AVX implementations");
        } else if (cpuinfo_has_x86_sse2()) {
            g_ActiveInstructionSet = InstructionSet::SSE2;
            CopyPixels = SSE2::CopyPixels;
            FastMemcpy = SSE2::FastMemcpy;
            logger::info("SIMDDispatch: Using SSE2 implementations");
        } else {
            g_ActiveInstructionSet = InstructionSet::None;
            CopyPixels = Generic::CopyPixels;
            FastMemcpy = Generic::FastMemcpy;
            logger::warn("SIMDDispatch: No SIMD support detected, using generic implementations");
        }
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
    }  // namespace Generic

}  // namespace PrismaUI::SIMD
