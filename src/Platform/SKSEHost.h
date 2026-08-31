#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

namespace PrismaUI::Platform::SKSEHost {
    using PluginHandle = std::uint32_t;
    inline constexpr PluginHandle kInvalidPluginHandle = static_cast<PluginHandle>(-1);

    enum class InterfaceId : std::uint32_t {
        kInvalid = 0,
        kScaleform,
        kPapyrus,
        kSerialization,
        kTask,
        kMessaging,
        kObject,
        kTrampoline
    };

    struct PluginInfo {
        std::uint32_t infoVersion;
        const char* name;
        std::uint32_t version;
    };

    struct LoadInterface {
        std::uint32_t skseVersion;
        std::uint32_t runtimeVersion;
        std::uint32_t editorVersion;
        std::uint32_t isEditor;
        void* (*QueryInterface)(std::uint32_t id);
        PluginHandle (*GetPluginHandle)();
        std::uint32_t (*GetReleaseIndex)();
        const PluginInfo* (*GetPluginInfo)(const char* name);
    };

    struct Message {
        const char* sender;
        std::uint32_t type;
        std::uint32_t dataLen;
        void* data;
    };

    using MessageCallback = void (*)(Message* message);

    enum class MessageType : std::uint32_t {
        kPostLoad,
        kPostPostLoad,
        kPreLoadGame,
        kPostLoadGame,
        kSaveGame,
        kDeleteGame,
        kInputLoaded,
        kNewGame,
        kDataLoaded
    };

    [[nodiscard]] bool Initialize(const LoadInterface* loadInterface) noexcept;
    [[nodiscard]] bool IsInitialized() noexcept;
    [[nodiscard]] PluginHandle GetPluginHandle() noexcept;
    [[nodiscard]] std::uint32_t RuntimeVersion() noexcept;
    [[nodiscard]] std::uint32_t SKSEVersion() noexcept;

    [[nodiscard]] bool RegisterListener(const char* sender, MessageCallback callback) noexcept;
    bool AddTask(std::function<void()> task) noexcept;
    bool AddUITask(std::function<void()> task) noexcept;
    [[nodiscard]] void* AllocateFromBranchPool(std::size_t size) noexcept;
    [[nodiscard]] void* AllocateFromLocalPool(std::size_t size) noexcept;
}
