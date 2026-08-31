#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PrismaUI::Platform {
    enum class AbiProfile : std::uint8_t { kUnknown, kSE_1_5, kAE_Pre629, kAE_629, kAE_1130, kAE_1_7, kVR_1_4_15 };

    enum class RuntimeFamily : std::uint8_t { kUnknown, kSE, kAE, kVR };

    struct RuntimeVersion {
        std::uint16_t major{0};
        std::uint16_t minor{0};
        std::uint16_t patch{0};
        std::uint16_t type{0};

        [[nodiscard]] static constexpr RuntimeVersion FromPacked(std::uint32_t packed) noexcept {
            return {static_cast<std::uint16_t>((packed >> 24) & 0xFF),
                    static_cast<std::uint16_t>((packed >> 16) & 0xFF),
                    static_cast<std::uint16_t>((packed >> 4) & 0xFFF), static_cast<std::uint16_t>(packed & 0xF)};
        }

        [[nodiscard]] std::string String(std::string_view separator = ".") const;
        [[nodiscard]] constexpr auto AsArray() const noexcept { return std::array{major, minor, patch, type}; }

        friend constexpr bool operator==(const RuntimeVersion&, const RuntimeVersion&) noexcept = default;
    };

    struct ModuleSection {
        std::string name;
        std::uintptr_t address{0};
        std::size_t size{0};

        [[nodiscard]] bool Contains(std::uintptr_t candidate) const noexcept;
    };

    class RuntimeContext {
    public:
        [[nodiscard]] static std::optional<RuntimeContext> Create(std::uint32_t packedRuntimeVersion,
                                                                  std::string& error);

        [[nodiscard]] RuntimeFamily Family() const noexcept { return family_; }
        [[nodiscard]] AbiProfile Profile() const noexcept { return profile_; }
        [[nodiscard]] const RuntimeVersion& Version() const noexcept { return version_; }
        [[nodiscard]] std::uintptr_t ModuleBase() const noexcept { return moduleBase_; }
        [[nodiscard]] const std::filesystem::path& ExecutablePath() const noexcept { return executablePath_; }
        [[nodiscard]] const std::vector<ModuleSection>& Sections() const noexcept { return sections_; }
        [[nodiscard]] const ModuleSection* FindSection(std::string_view name) const noexcept;

    private:
        RuntimeFamily family_{RuntimeFamily::kUnknown};
        AbiProfile profile_{AbiProfile::kUnknown};
        RuntimeVersion version_;
        std::uintptr_t moduleBase_{0};
        std::filesystem::path executablePath_;
        std::vector<ModuleSection> sections_;
    };
}
