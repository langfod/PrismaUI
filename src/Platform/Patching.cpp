#include "Platform/Patching.h"

#include <Windows.h>

#include <array>
#include <cstring>
#include <limits>

#include "Platform/SKSEHost.h"


namespace PrismaUI::Platform::Patching {
    namespace {
#pragma pack(push, 1)
        struct RelativeCall {
            std::uint8_t opcode;
            std::int32_t displacement;
        };

        struct AbsoluteJump {
            std::uint8_t opcode0;
            std::uint8_t opcode1;
            std::uint32_t displacement;
            std::uint64_t destination;
        };
#pragma pack(pop)

        static_assert(sizeof(RelativeCall) == 5);
        static_assert(sizeof(AbsoluteJump) == 14);

        class WritableRegion {
        public:
            WritableRegion(void* address, std::size_t size) noexcept : address_(address), size_(size) {
                valid_ = VirtualProtect(address_, size_, PAGE_EXECUTE_READWRITE, &oldProtection_) != FALSE;
            }

            ~WritableRegion() {
                if (valid_) {
                    DWORD ignored = 0;
                    VirtualProtect(address_, size_, oldProtection_, &ignored);
                }
            }

            [[nodiscard]] bool IsValid() const noexcept { return valid_; }

        private:
            void* address_;
            std::size_t size_;
            DWORD oldProtection_{0};
            bool valid_{false};
        };
    }

    std::optional<PreparedCall5> PrepareCall5(std::uintptr_t source, std::uintptr_t destination,
                                              std::string& error) noexcept {
        if (source == 0 || destination == 0) {
            error = "Cannot patch a null call address";
            return std::nullopt;
        }

        RelativeCall original{};
        std::memcpy(&original, reinterpret_cast<const void*>(source), sizeof(original));
        if (original.opcode != 0xE8) {
            error = "The D3D hook site is not a five-byte relative call";
            return std::nullopt;
        }

        const auto originalTarget = source + sizeof(RelativeCall) + original.displacement;
        auto* trampolineMemory = SKSEHost::AllocateFromBranchPool(sizeof(AbsoluteJump));
        if (!trampolineMemory) {
            error = "SKSE branch trampoline allocation failed";
            return std::nullopt;
        }

        const auto trampolineAddress = reinterpret_cast<std::uintptr_t>(trampolineMemory);
        const auto nextInstruction = source + sizeof(RelativeCall);
        const auto displacement =
            static_cast<std::int64_t>(trampolineAddress) - static_cast<std::int64_t>(nextInstruction);
        if (displacement < std::numeric_limits<std::int32_t>::min() ||
            displacement > std::numeric_limits<std::int32_t>::max()) {
            error = "SKSE branch trampoline is outside rel32 range";
            return std::nullopt;
        }

        const AbsoluteJump trampoline{0xFF, 0x25, 0, destination};
        std::memcpy(trampolineMemory, &trampoline, sizeof(trampoline));
        FlushInstructionCache(GetCurrentProcess(), trampolineMemory, sizeof(trampoline));

        const RelativeCall replacement{0xE8, static_cast<std::int32_t>(displacement)};
        PreparedCall5 prepared{source, originalTarget};
        std::memcpy(prepared.replacement.data(), &replacement, sizeof(replacement));
        error.clear();
        return prepared;
    }

    bool ApplyCall5(const PreparedCall5& patch, std::string& error) noexcept {
        if (patch.source == 0 || patch.originalTarget == 0) {
            error = "Cannot apply an invalid five-byte call patch";
            return false;
        }

        WritableRegion writable(reinterpret_cast<void*>(patch.source), patch.replacement.size());
        if (!writable.IsValid()) {
            error = "VirtualProtect failed while patching the D3D call";
            return false;
        }
        std::memcpy(reinterpret_cast<void*>(patch.source), patch.replacement.data(), patch.replacement.size());
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(patch.source), patch.replacement.size());

        error.clear();
        return true;
    }
}
