#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Platform/RuntimeContext.h"


namespace PrismaUI::Platform {
    class AddressLibrary {
    public:
        [[nodiscard]] bool Load(const RuntimeContext& runtime, const std::filesystem::path& gameDirectory,
                                std::string& error);
        [[nodiscard]] bool LoadFile(const std::filesystem::path& path, RuntimeFamily family,
                                    const RuntimeVersion& runtimeVersion, std::string& error);
        [[nodiscard]] std::optional<std::uint64_t> Offset(std::uint64_t id) const noexcept;
        [[nodiscard]] bool IsLoaded() const noexcept { return loaded_; }
        [[nodiscard]] const std::filesystem::path& LoadedPath() const noexcept { return loadedPath_; }

        [[nodiscard]] static std::filesystem::path ExpectedPath(const RuntimeContext& runtime,
                                                                const std::filesystem::path& gameDirectory);

    private:
        [[nodiscard]] bool LoadBinary(const std::filesystem::path& path, const RuntimeVersion& runtimeVersion,
                                      std::string& error);
        [[nodiscard]] bool LoadCsv(const std::filesystem::path& path, std::string& error);

        std::unordered_map<std::uint64_t, std::uint64_t> sparseOffsets_;
        std::vector<std::uint32_t> denseOffsets_;
        std::filesystem::path loadedPath_;
        bool loaded_{false};
    };
}
