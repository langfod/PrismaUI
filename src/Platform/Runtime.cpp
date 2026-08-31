#include "Platform/Runtime.h"

#include <memory>
#include <stdexcept>

namespace PrismaUI::Platform {
    namespace {
        std::unique_ptr<RuntimeContext> g_runtime;
        std::unique_ptr<AddressLibrary> g_addressLibrary;
        std::unique_ptr<RelocationResolver> g_relocations;
    }

    bool InitializeRuntime(std::uint32_t packedRuntimeVersion, std::string& error) {
        auto runtime = RuntimeContext::Create(packedRuntimeVersion, error);
        if (!runtime) {
            return false;
        }

        auto addressLibrary = std::make_unique<AddressLibrary>();
        if (!addressLibrary->Load(*runtime, runtime->ExecutablePath().parent_path(), error)) {
            return false;
        }

        auto relocations = std::make_unique<RelocationResolver>();
        if (!relocations->Initialize(*runtime, *addressLibrary, error)) {
            return false;
        }

        g_runtime = std::make_unique<RuntimeContext>(std::move(*runtime));
        g_addressLibrary = std::move(addressLibrary);
        g_relocations = std::move(relocations);
        error.clear();
        return true;
    }

    bool IsRuntimeInitialized() noexcept { return g_runtime && g_addressLibrary && g_relocations; }

    const RuntimeContext& GetRuntimeContext() {
        if (!g_runtime) {
            throw std::logic_error("PrismaUI runtime context has not been initialized");
        }
        return *g_runtime;
    }

    const RelocationResolver& GetRelocations() {
        if (!g_relocations) {
            throw std::logic_error("PrismaUI relocations have not been initialized");
        }
        return *g_relocations;
    }
}
