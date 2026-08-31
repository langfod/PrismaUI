#include <cstddef>
#include <cstdint>

namespace {
    constexpr std::uint32_t PackVersion(std::uint32_t major, std::uint32_t minor, std::uint32_t patch,
                                        std::uint32_t type = 0) noexcept {
        return ((major & 0xFF) << 24) | ((minor & 0xFF) << 16) | ((patch & 0xFFF) << 4) | (type & 0xF);
    }

    struct PluginVersionData {
        std::uint32_t dataVersion;
        std::uint32_t pluginVersion;
        char name[256];
        char author[256];
        char supportEmail[252];
        std::uint32_t versionIndependenceEx;
        std::uint32_t versionIndependence;
        std::uint32_t compatibleVersions[16];
        std::uint32_t skseVersionRequired;
    };
    static_assert(offsetof(PluginVersionData, versionIndependenceEx) == 0x304);
    static_assert(offsetof(PluginVersionData, versionIndependence) == 0x308);
    static_assert(offsetof(PluginVersionData, compatibleVersions) == 0x30C);
    static_assert(offsetof(PluginVersionData, skseVersionRequired) == 0x34C);
    static_assert(sizeof(PluginVersionData) == 0x350);
}

extern "C" __declspec(dllexport) constinit PluginVersionData SKSEPlugin_Version{
    1,
    PackVersion(1, 5, 0),
    "PrismaUI",
    "StarkMP <discord: starkmp>",
    "",
    (1U << 0) | (1U << 1),  // Runtime-profiled structures and Address Library v5 support.
    1U << 0,                // Address Library version independence.
    {},
    PackVersion(2, 0, 1, 4)};
