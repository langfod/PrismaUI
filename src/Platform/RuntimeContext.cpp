#include "Platform/RuntimeContext.h"

#include <Windows.h>

#include <algorithm>
#include <format>
#include <limits>

namespace PrismaUI::Platform {
    namespace {
        bool Matches(const RuntimeVersion& version, std::uint16_t major, std::uint16_t minor,
                     std::uint16_t patch) noexcept {
            return version.major == major && version.minor == minor && version.patch == patch;
        }

        AbiProfile SelectProfile(const RuntimeVersion& version) noexcept {
            if (Matches(version, 1, 4, 15)) {
                return AbiProfile::kVR_1_4_15;
            }
            if (version.major == 1 && version.minor == 5 && version.patch >= 3 && version.patch <= 97) {
                return AbiProfile::kSE_1_5;
            }
            if (version.major == 1 && version.minor == 6) {
                switch (version.patch) {
                    case 317:
                    case 318:
                    case 323:
                    case 342:
                    case 353:
                        return AbiProfile::kAE_Pre629;
                    case 629:
                    case 640:
                    case 659:
                    case 678:
                        return AbiProfile::kAE_629;
                    case 1130:
                    case 1170:
                    case 1179:
                        return AbiProfile::kAE_1130;
                    default:
                        return AbiProfile::kUnknown;
                }
            }
            if (version.major == 1 && version.minor == 7 && (version.patch == 99 || version.patch == 104)) {
                return AbiProfile::kAE_1_7;
            }
            return AbiProfile::kUnknown;
        }
    }

    std::string RuntimeVersion::String(std::string_view separator) const {
        return std::format("{}{}{}{}{}{}{}", major, separator, minor, separator, patch, separator, type);
    }

    bool ModuleSection::Contains(std::uintptr_t candidate) const noexcept {
        return address != 0 && candidate >= address && candidate - address < size;
    }

    std::optional<RuntimeContext> RuntimeContext::Create(std::uint32_t packedRuntimeVersion, std::string& error) {
        RuntimeContext context;
        context.version_ = RuntimeVersion::FromPacked(packedRuntimeVersion);
        if (context.version_.major != 1) {
            error = std::format("Unsupported Skyrim runtime version {}", context.version_.String());
            return std::nullopt;
        }

        if (context.version_.minor == 4) {
            context.family_ = RuntimeFamily::kVR;
        } else if (context.version_.minor == 5) {
            context.family_ = RuntimeFamily::kSE;
        } else if (context.version_.minor >= 6) {
            context.family_ = RuntimeFamily::kAE;
        } else {
            error = std::format("Unsupported Skyrim runtime family {}", context.version_.String());
            return std::nullopt;
        }

        context.profile_ = SelectProfile(context.version_);
        if (context.profile_ == AbiProfile::kUnknown) {
            error = std::format("Skyrim runtime {} has no validated PrismaUI ABI profile", context.version_.String());
            return std::nullopt;
        }

        const auto module = GetModuleHandleW(nullptr);
        if (!module) {
            error = std::format("GetModuleHandleW failed with error {}", GetLastError());
            return std::nullopt;
        }
        context.moduleBase_ = reinterpret_cast<std::uintptr_t>(module);

        std::wstring path(32768, L'\0');
        const auto pathLength = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (pathLength == 0 || pathLength == path.size()) {
            error = std::format("GetModuleFileNameW failed with error {}", GetLastError());
            return std::nullopt;
        }
        path.resize(pathLength);
        context.executablePath_ = std::move(path);

        const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(context.moduleBase_);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew <= 0) {
            error = "The host executable has an invalid DOS header";
            return std::nullopt;
        }

        const auto ntAddress = context.moduleBase_ + static_cast<std::uintptr_t>(dosHeader->e_lfanew);
        const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(ntAddress);
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
            ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            error = "The host executable has an invalid 64-bit PE header";
            return std::nullopt;
        }

        const auto* section = IMAGE_FIRST_SECTION(ntHeaders);
        const auto sectionCount = static_cast<std::size_t>(ntHeaders->FileHeader.NumberOfSections);
        context.sections_.reserve(sectionCount);
        for (std::size_t index = 0; index < sectionCount; ++index) {
            const auto nameLength = std::find(std::begin(section[index].Name), std::end(section[index].Name), '\0') -
                                    std::begin(section[index].Name);
            context.sections_.push_back(
                {std::string(reinterpret_cast<const char*>(section[index].Name), nameLength),
                 context.moduleBase_ + section[index].VirtualAddress,
                 static_cast<std::size_t>(section[index].Misc.VirtualSize)});
        }

        error.clear();
        return context;
    }

    const ModuleSection* RuntimeContext::FindSection(std::string_view name) const noexcept {
        const auto it = std::find_if(sections_.begin(), sections_.end(),
                                     [name](const ModuleSection& section) { return section.name == name; });
        return it == sections_.end() ? nullptr : std::addressof(*it);
    }
}
