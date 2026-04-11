#pragma once
#include <cstdint>
#include <malloc.h>
#include <new>
#include <type_traits>

namespace PrismaUI::Utils {

    /**
     * Custom allocator that ensures memory is aligned to the specified boundary.
     * Used for SIMD operations where alignment improves performance dramatically.
     * 
     * @tparam T The type to allocate
     * @tparam Alignment Alignment boundary in bytes (default 32 for AVX2)
     * 
     * Alignment notes:
     * - 32 bytes: Optimal for AVX2, also works for AVX and SSE2
     * - 16 bytes: Required minimum for SSE2/AVX, but 32 is recommended for AVX2
     * - Using 32-byte alignment does NOT hurt performance on SSE2/AVX systems
     */
    template<typename T, size_t Alignment = 32>
    struct AlignedAllocator {
        static_assert(Alignment > 0, "Alignment must be greater than 0");
        static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of 2");
        static_assert(Alignment >= alignof(T), "Alignment must be at least alignof(T)");

        using value_type = T;

        // Required: MSVC's allocator_traits cannot auto-rebind when a non-type
        // template parameter (size_t Alignment) is present.
        template<typename U>
        struct rebind { using other = AlignedAllocator<U, Alignment>; };

        AlignedAllocator() noexcept = default;

        template<typename U>
        AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

        T* allocate(size_t n) {
            if (n == 0) return nullptr;

            if (n > SIZE_MAX / sizeof(T)) throw std::bad_alloc();

            size_t size = n * sizeof(T);
            void* ptr = _aligned_malloc(size, Alignment);

            if (!ptr) throw std::bad_alloc();

            return static_cast<T*>(ptr);
        }

        void deallocate(T* ptr, size_t) noexcept {
            if (ptr) _aligned_free(ptr);
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
