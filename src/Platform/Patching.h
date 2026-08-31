#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace PrismaUI::Platform::Patching {
    struct PreparedCall5 {
        std::uintptr_t source{0};
        std::uintptr_t originalTarget{0};
        std::array<std::byte, 5> replacement{};
    };

    [[nodiscard]] std::optional<PreparedCall5> PrepareCall5(std::uintptr_t source, std::uintptr_t destination,
                                                            std::string& error) noexcept;
    [[nodiscard]] bool ApplyCall5(const PreparedCall5& patch, std::string& error) noexcept;
}
