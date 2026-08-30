#include "Platform/AddressLibrary.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <stdexcept>

namespace PrismaUI::Platform {
    namespace {
        template <class T>
        bool Read(std::istream& input, T& value) {
            return static_cast<bool>(input.read(reinterpret_cast<char*>(std::addressof(value)), sizeof(value)));
        }

        bool ReadBytes(std::istream& input, std::size_t count) {
            if (count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
                return false;
            }
            input.ignore(static_cast<std::streamsize>(count));
            return static_cast<bool>(input);
        }

        std::string Trim(std::string value) {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                return {};
            }
            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        template <class T>
        bool ParseInteger(std::string_view text, T& value, int base = 10) {
            const auto* first = text.data();
            const auto* last = first + text.size();
            const auto result = std::from_chars(first, last, value, base);
            return result.ec == std::errc{} && result.ptr == last;
        }

        bool VersionMatches(const std::array<std::uint32_t, 4>& actual, const RuntimeVersion& expected) {
            const auto expectedParts = expected.AsArray();
            return std::equal(actual.begin(), actual.end(), expectedParts.begin());
        }

        std::optional<std::uint64_t> RemainingBytes(std::istream& input) {
            const auto current = input.tellg();
            if (current < 0) {
                return std::nullopt;
            }
            input.seekg(0, std::ios::end);
            const auto end = input.tellg();
            input.seekg(current);
            if (!input || end < current) {
                return std::nullopt;
            }
            return static_cast<std::uint64_t>(end - current);
        }
    }

    std::filesystem::path AddressLibrary::ExpectedPath(const RuntimeContext& runtime,
                                                       const std::filesystem::path& gameDirectory) {
        const auto version = runtime.Version().String("-");
        std::string filename;
        switch (runtime.Family()) {
            case RuntimeFamily::kSE:
                filename = "version-" + version + ".bin";
                break;
            case RuntimeFamily::kAE:
                filename = "versionlib-" + version + ".bin";
                break;
            case RuntimeFamily::kVR:
                filename = "version-" + version + ".csv";
                break;
            default:
                return {};
        }
        return gameDirectory / "Data" / "SKSE" / "Plugins" / filename;
    }

    bool AddressLibrary::Load(const RuntimeContext& runtime, const std::filesystem::path& gameDirectory,
                              std::string& error) {
        try {
            return LoadFile(ExpectedPath(runtime, gameDirectory), runtime.Family(), runtime.Version(), error);
        } catch (const std::exception& exception) {
            error = "Failed to prepare the Address Library path: " + std::string(exception.what());
            return false;
        }
    }

    bool AddressLibrary::LoadFile(const std::filesystem::path& path, RuntimeFamily family,
                                  const RuntimeVersion& runtimeVersion, std::string& error) {
        try {
            sparseOffsets_.clear();
            denseOffsets_.clear();
            loadedPath_.clear();
            loaded_ = false;

            if (path.empty() || !std::filesystem::is_regular_file(path)) {
                error = "Required Address Library file was not found: " + path.string();
                return false;
            }

            const bool success =
                family == RuntimeFamily::kVR ? LoadCsv(path, error) : LoadBinary(path, runtimeVersion, error);
            if (success) {
                loadedPath_ = path;
                loaded_ = true;
                error.clear();
            }
            return success;
        } catch (const std::bad_alloc&) {
            error = "Address Library allocation failed while parsing: " + path.string();
        } catch (const std::length_error& exception) {
            error = "Address Library size is invalid: " + std::string(exception.what());
        } catch (const std::exception& exception) {
            error = "Address Library parsing failed: " + std::string(exception.what());
        }
        sparseOffsets_.clear();
        denseOffsets_.clear();
        loadedPath_.clear();
        loaded_ = false;
        return false;
    }

    std::optional<std::uint64_t> AddressLibrary::Offset(std::uint64_t id) const noexcept {
        if (!loaded_) {
            return std::nullopt;
        }
        if (!denseOffsets_.empty()) {
            if (id >= denseOffsets_.size() || denseOffsets_[static_cast<std::size_t>(id)] == 0) {
                return std::nullopt;
            }
            return denseOffsets_[static_cast<std::size_t>(id)];
        }
        const auto it = sparseOffsets_.find(id);
        return it == sparseOffsets_.end() ? std::nullopt : std::optional<std::uint64_t>{it->second};
    }

    bool AddressLibrary::LoadBinary(const std::filesystem::path& path, const RuntimeVersion& runtimeVersion,
                                    std::string& error) {
        std::ifstream input(path, std::ios::binary);
        std::int32_t format = 0;
        if (!input || !Read(input, format)) {
            error = "Address Library header is truncated: " + path.string();
            return false;
        }

        if (format == 5) {
            std::array<std::uint32_t, 4> version{};
            std::array<char, 64> name{};
            std::int32_t pointerSize = 0;
            std::int32_t dataFormat = 0;
            std::int32_t offsetCount = 0;
            if (!Read(input, version) || !Read(input, name) || !Read(input, pointerSize) || !Read(input, dataFormat) ||
                !Read(input, offsetCount)) {
                error = "Address Library v5 header is truncated: " + path.string();
                return false;
            }
            if (!VersionMatches(version, runtimeVersion) || pointerSize != 8 || offsetCount <= 0) {
                error = "Address Library v5 header does not match the running game: " + path.string();
                return false;
            }
            const auto count = static_cast<std::size_t>(offsetCount);
            const auto remaining = RemainingBytes(input);
            if (!remaining || count > *remaining / sizeof(std::uint32_t)) {
                error = "Address Library v5 offset count exceeds the remaining file size";
                return false;
            }
            denseOffsets_.resize(count);
            if (!input.read(reinterpret_cast<char*>(denseOffsets_.data()),
                            static_cast<std::streamsize>(count * sizeof(std::uint32_t)))) {
                denseOffsets_.clear();
                error = "Address Library v5 body is truncated: " + path.string();
                return false;
            }
            return true;
        }

        if (format != 1 && format != 2) {
            error = "Unsupported Address Library binary format " + std::to_string(format);
            return false;
        }

        std::array<std::int32_t, 4> signedVersion{};
        std::int32_t nameLength = 0;
        std::int32_t pointerSize = 0;
        std::int32_t addressCount = 0;
        if (!Read(input, signedVersion) || !Read(input, nameLength) || nameLength < 0 ||
            !ReadBytes(input, static_cast<std::size_t>(nameLength)) || !Read(input, pointerSize) ||
            !Read(input, addressCount)) {
            error = "Address Library sparse header is invalid: " + path.string();
            return false;
        }
        std::array<std::uint32_t, 4> version{};
        std::transform(signedVersion.begin(), signedVersion.end(), version.begin(),
                       [](std::int32_t part) { return static_cast<std::uint32_t>(part); });
        if (!VersionMatches(version, runtimeVersion) || pointerSize <= 0 || addressCount <= 0) {
            error = "Address Library sparse header does not match the running game: " + path.string();
            return false;
        }
        const auto remaining = RemainingBytes(input);
        if (!remaining || static_cast<std::uint64_t>(addressCount) > *remaining) {
            error = "Address Library sparse address count exceeds the remaining file size";
            return false;
        }

        std::uint64_t previousId = 0;
        std::uint64_t previousOffset = 0;
        sparseOffsets_.reserve(static_cast<std::size_t>(addressCount));
        for (std::int32_t index = 0; index < addressCount; ++index) {
            std::uint8_t type = 0;
            if (!Read(input, type)) {
                error = "Address Library sparse body is truncated";
                return false;
            }

            auto decode = [&input](std::uint8_t encoding, std::uint64_t previous, std::uint64_t& result) -> bool {
                std::uint8_t value8 = 0;
                std::uint16_t value16 = 0;
                std::uint32_t value32 = 0;
                switch (encoding) {
                    case 0:
                        return Read(input, result);
                    case 1:
                        result = previous + 1;
                        return true;
                    case 2:
                        if (!Read(input, value8)) return false;
                        result = previous + value8;
                        return true;
                    case 3:
                        if (!Read(input, value8) || value8 > previous) return false;
                        result = previous - value8;
                        return true;
                    case 4:
                        if (!Read(input, value16)) return false;
                        result = previous + value16;
                        return true;
                    case 5:
                        if (!Read(input, value16) || value16 > previous) return false;
                        result = previous - value16;
                        return true;
                    case 6:
                        if (!Read(input, value16)) return false;
                        result = value16;
                        return true;
                    case 7:
                        if (!Read(input, value32)) return false;
                        result = value32;
                        return true;
                    default:
                        return false;
                }
            };

            std::uint64_t id = 0;
            std::uint64_t offset = 0;
            if (!decode(type & 0xF, previousId, id)) {
                error = "Address Library contains an invalid ID encoding";
                return false;
            }
            const bool scaled = (type & 0x80) != 0;
            const auto previousBase =
                scaled ? previousOffset / static_cast<std::uint64_t>(pointerSize) : previousOffset;
            if (!decode((type >> 4) & 0x7, previousBase, offset)) {
                error = "Address Library contains an invalid offset encoding";
                return false;
            }
            if (scaled) {
                if (offset > std::numeric_limits<std::uint64_t>::max() / static_cast<std::uint64_t>(pointerSize)) {
                    error = "Address Library offset multiplication overflowed";
                    return false;
                }
                offset *= static_cast<std::uint64_t>(pointerSize);
            }
            if (!sparseOffsets_.emplace(id, offset).second) {
                error = "Address Library contains a duplicate ID " + std::to_string(id);
                return false;
            }
            previousId = id;
            previousOffset = offset;
        }
        return true;
    }

    bool AddressLibrary::LoadCsv(const std::filesystem::path& path, std::string& error) {
        std::ifstream input(path);
        std::string line;
        if (!input || !std::getline(input, line) || Trim(line) != "id,offset") {
            error = "VR Address Library CSV header is invalid: " + path.string();
            return false;
        }
        if (!std::getline(input, line)) {
            error = "VR Address Library CSV is missing its metadata row";
            return false;
        }
        const auto metadataComma = line.find(',');
        if (metadataComma == std::string::npos) {
            error = "VR Address Library CSV metadata row is invalid";
            return false;
        }
        std::uint64_t expectedCount = 0;
        const auto countText = Trim(line.substr(0, metadataComma));
        if (!ParseInteger<std::uint64_t>(countText, expectedCount) || expectedCount == 0 ||
            expectedCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            error = "VR Address Library CSV entry count is invalid";
            return false;
        }
        const auto remaining = RemainingBytes(input);
        constexpr std::uint64_t kMinimumCsvRowBytes = 3;  // "0,0"
        if (!remaining || expectedCount > *remaining / kMinimumCsvRowBytes) {
            error = "VR Address Library CSV entry count exceeds the remaining file size";
            return false;
        }
        sparseOffsets_.reserve(static_cast<std::size_t>(expectedCount));

        std::uint64_t rowCount = 0;
        while (std::getline(input, line)) {
            line = Trim(std::move(line));
            if (line.empty()) {
                continue;
            }
            const auto comma = line.find(',');
            if (comma == std::string::npos) {
                error = "VR Address Library CSV contains an invalid row";
                return false;
            }
            std::uint64_t id = 0;
            std::uint64_t offset = 0;
            const auto idText = Trim(line.substr(0, comma));
            const auto offsetText = Trim(line.substr(comma + 1));
            if (!ParseInteger<std::uint64_t>(idText, id) || !ParseInteger<std::uint64_t>(offsetText, offset, 16)) {
                error = "VR Address Library CSV contains a non-numeric row";
                return false;
            }
            if (!sparseOffsets_.emplace(id, offset).second) {
                error = "VR Address Library CSV contains a duplicate ID " + std::to_string(id);
                return false;
            }
            ++rowCount;
        }
        if (rowCount != expectedCount) {
            error = "VR Address Library CSV entry count does not match its metadata";
            return false;
        }
        return true;
    }
}
