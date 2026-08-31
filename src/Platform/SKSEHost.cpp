#include "Platform/SKSEHost.h"

#include <Windows.h>

#include <atomic>
#include <new>

namespace PrismaUI::Platform::SKSEHost {
    namespace {
        struct MessagingInterface {
            std::uint32_t interfaceVersion;
            bool (*RegisterListener)(PluginHandle listener, const char* sender, MessageCallback callback);
            bool (*Dispatch)(PluginHandle sender, std::uint32_t type, void* data, std::uint32_t dataLen,
                             const char* receiver);
            void* (*GetEventDispatcher)(std::uint32_t dispatcherId);
        };

        struct TaskInterface {
            std::uint32_t interfaceVersion;
            void (*AddTask)(void* task);
            void (*AddUITask)(void* task);
        };

        struct TrampolineInterface {
            std::uint32_t interfaceVersion;
            void* (*AllocateFromBranchPool)(PluginHandle plugin, std::size_t size);
            void* (*AllocateFromLocalPool)(PluginHandle plugin, std::size_t size);
        };

        class TaskDelegate {
        public:
            virtual void Run() = 0;
            virtual void Dispose() = 0;

        protected:
            ~TaskDelegate() = default;
        };

        class UIDelegate {
        public:
            virtual void Run() = 0;
            virtual void Dispose() = 0;

        protected:
            ~UIDelegate() = default;
        };

        template <class Base>
        class FunctionDelegate final : public Base {
        public:
            explicit FunctionDelegate(std::function<void()> task) : task_(std::move(task)) {}

            void Run() override {
                try {
                    task_();
                } catch (...) {
                    OutputDebugStringA("PrismaUI: an exception escaped a queued SKSE task\n");
                }
            }

            void Dispose() override { delete this; }

        private:
            std::function<void()> task_;
        };

        const LoadInterface* g_loadInterface = nullptr;
        MessagingInterface* g_messaging = nullptr;
        TaskInterface* g_tasks = nullptr;
        TrampolineInterface* g_trampoline = nullptr;
        PluginHandle g_pluginHandle = kInvalidPluginHandle;
        std::atomic_bool g_initialized{false};

        template <class T>
        T* Query(InterfaceId id) noexcept {
            if (!g_loadInterface || !g_loadInterface->QueryInterface) {
                return nullptr;
            }
            return static_cast<T*>(g_loadInterface->QueryInterface(static_cast<std::uint32_t>(id)));
        }
    }

    bool Initialize(const LoadInterface* loadInterface) noexcept {
        g_initialized.store(false, std::memory_order_release);
        g_loadInterface = loadInterface;
        g_messaging = nullptr;
        g_tasks = nullptr;
        g_trampoline = nullptr;
        g_pluginHandle = kInvalidPluginHandle;

        if (!g_loadInterface || !g_loadInterface->QueryInterface || !g_loadInterface->GetPluginHandle) {
            return false;
        }

        g_pluginHandle = g_loadInterface->GetPluginHandle();
        g_messaging = Query<MessagingInterface>(InterfaceId::kMessaging);
        g_tasks = Query<TaskInterface>(InterfaceId::kTask);
        g_trampoline = Query<TrampolineInterface>(InterfaceId::kTrampoline);
        if (g_pluginHandle == kInvalidPluginHandle || !g_messaging || g_messaging->interfaceVersion < 2 || !g_tasks ||
            g_tasks->interfaceVersion < 2 || !g_trampoline || g_trampoline->interfaceVersion < 1) {
            return false;
        }

        g_initialized.store(true, std::memory_order_release);
        return true;
    }

    bool IsInitialized() noexcept { return g_initialized.load(std::memory_order_acquire); }

    PluginHandle GetPluginHandle() noexcept { return g_pluginHandle; }

    std::uint32_t RuntimeVersion() noexcept { return g_loadInterface ? g_loadInterface->runtimeVersion : 0; }

    std::uint32_t SKSEVersion() noexcept { return g_loadInterface ? g_loadInterface->skseVersion : 0; }

    bool RegisterListener(const char* sender, MessageCallback callback) noexcept {
        return IsInitialized() && sender && callback && g_messaging->RegisterListener(g_pluginHandle, sender, callback);
    }

    bool AddTask(std::function<void()> task) noexcept {
        if (!IsInitialized() || !task) {
            return false;
        }
        auto* delegate = new (std::nothrow) FunctionDelegate<TaskDelegate>(std::move(task));
        if (!delegate) {
            return false;
        }
        g_tasks->AddTask(delegate);
        return true;
    }

    bool AddUITask(std::function<void()> task) noexcept {
        if (!IsInitialized() || !task) {
            return false;
        }
        auto* delegate = new (std::nothrow) FunctionDelegate<UIDelegate>(std::move(task));
        if (!delegate) {
            return false;
        }
        g_tasks->AddUITask(delegate);
        return true;
    }

    void* AllocateFromBranchPool(std::size_t size) noexcept {
        return IsInitialized() && size > 0 ? g_trampoline->AllocateFromBranchPool(g_pluginHandle, size) : nullptr;
    }

    void* AllocateFromLocalPool(std::size_t size) noexcept {
        return IsInitialized() && size > 0 ? g_trampoline->AllocateFromLocalPool(g_pluginHandle, size) : nullptr;
    }
}
