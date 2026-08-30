#include "Platform/Relocation.h"

#include <format>
#include <limits>

namespace PrismaUI::Platform {
    bool RelocationResolver::Initialize(const RuntimeContext& runtime, const AddressLibrary& addressLibrary,
                                        std::string& error) {
        std::unordered_map<AddressKey, std::uintptr_t> resolved;
        resolved.reserve(std::size(kAddressCatalog));

        for (const auto& spec : kAddressCatalog) {
            std::uint64_t id = 0;
            std::uint32_t addend = 0;
            switch (runtime.Family()) {
                case RuntimeFamily::kSE:
                    id = spec.seId;
                    addend = spec.seAddend;
                    break;
                case RuntimeFamily::kAE:
                    id = spec.aeId;
                    addend = spec.aeAddend;
                    break;
                case RuntimeFamily::kVR:
                    id = spec.vrId;
                    addend = spec.vrAddend;
                    break;
                default:
                    error = "Cannot resolve addresses for an unknown runtime family";
                    return false;
            }

            const auto offset = addressLibrary.Offset(id);
            if (!offset) {
                error = std::format("Address Library does not contain required ID {} ({})", id, spec.name);
                return false;
            }
            if (*offset > std::numeric_limits<std::uintptr_t>::max() - runtime.ModuleBase() - addend) {
                error = std::format("Resolved address overflow for {}", spec.name);
                return false;
            }
            const auto address = runtime.ModuleBase() + static_cast<std::uintptr_t>(*offset) + addend;
            const auto* section = runtime.FindSection(spec.expectedSection);
            if (!section || !section->Contains(address)) {
                error = std::format("Resolved address for {} is outside expected section {}", spec.name,
                                    spec.expectedSection);
                return false;
            }
            resolved.emplace(spec.key, address);
        }

        addresses_ = std::move(resolved);
        error.clear();
        return true;
    }

    std::optional<std::uintptr_t> RelocationResolver::Address(AddressKey key) const noexcept {
        const auto it = addresses_.find(key);
        return it == addresses_.end() ? std::nullopt : std::optional<std::uintptr_t>{it->second};
    }
}
