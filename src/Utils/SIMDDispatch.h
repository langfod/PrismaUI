#pragma once

#include <cstdint>
#include <cstddef>

namespace PrismaUI::SIMD {

    /**
     * SIMD instruction set levels
     */
    enum class InstructionSet {
        None,       // No SIMD (generic C++)
        SSE2,       // Baseline x86_64 support
        AVX,        // AVX (2011+)
        AVX2        // AVX2 (2013+)
    };

    /**
     * Initialize SIMD dispatcher - detects CPU capabilities and selects best implementations.
     * Call this once at startup before using any SIMD functions.
     */
    void Initialize();

    /**
     * Get the currently active instruction set
     */
    InstructionSet GetActiveInstructionSet();

    /**
     * Get human-readable name for instruction set
     */
    const char* GetInstructionSetName(InstructionSet set);

    // ============================================================
    // SIMD-optimized functions with runtime dispatch
    // ============================================================

    /**
     * Copy pixel data with optimal SIMD instructions.
     * Handles misaligned data and varying row pitches.
     * 
     * @param dest Destination buffer
     * @param destPitch Bytes per row in destination
     * @param src Source buffer
     * @param srcPitch Bytes per row in source
     * @param width Image width in pixels (assumes BGRA, 4 bytes per pixel)
     * @param height Image height in rows
     */
    using CopyPixelsFunc = void (*)(void* dest, uint32_t destPitch, const void* src, uint32_t srcPitch,
                                     uint32_t width, uint32_t height);
    extern CopyPixelsFunc CopyPixels;

    /**
     * Fast memcpy optimized for large texture data.
     * Uses largest available SIMD instructions.
     * 
     * @param dest Destination buffer
     * @param src Source buffer  
     * @param size Number of bytes to copy
     */
    using FastMemcpyFunc = void (*)(void* dest, const void* src, size_t size);
    extern FastMemcpyFunc FastMemcpy;

    /**
     * Swizzle RGBA -> BGRA, force alpha to 255, and flip vertically.
     * Used by the WebGL readback path to convert ANGLE's glReadPixels output
     * into the format expected by the D3D11 shared texture.
     *
     * @param dest   Destination buffer (top-to-bottom row order, BGRA)
     * @param src    Source buffer (bottom-to-top row order, RGBA)
     * @param width  Image width in pixels
     * @param height Image height in rows
     */
    using SwizzleFlipPixelsFunc = void (*)(void* dest, const void* src,
                                            uint32_t width, uint32_t height);
    extern SwizzleFlipPixelsFunc SwizzleFlipPixels;

}  // namespace PrismaUI::SIMD
