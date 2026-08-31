#pragma once

#include <cstdint>
#include <string>

#include "Platform/AddressLibrary.h"
#include "Platform/Relocation.h"


namespace PrismaUI::Platform {
    [[nodiscard]] bool InitializeRuntime(std::uint32_t packedRuntimeVersion, std::string& error);
    [[nodiscard]] bool IsRuntimeInitialized() noexcept;
    [[nodiscard]] const RuntimeContext& GetRuntimeContext();
    [[nodiscard]] const RelocationResolver& GetRelocations();
}
