#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "Platform/AddressLibrary.h"
#include "Platform/Addresses.h"


namespace PrismaUI::Platform {
    class RelocationResolver {
    public:
        [[nodiscard]] bool Initialize(const RuntimeContext& runtime, const AddressLibrary& addressLibrary,
                                      std::string& error);
        [[nodiscard]] std::optional<std::uintptr_t> Address(AddressKey key) const noexcept;

    private:
        std::unordered_map<AddressKey, std::uintptr_t> addresses_;
    };
}
