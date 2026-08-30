#include <spdlog/sinks/ostream_sink.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "Platform/AddressLibrary.h"
#include "Platform/Logging.h"


namespace {
    template <class T>
    void Write(std::ostream& output, const T& value) {
        output.write(reinterpret_cast<const char*>(std::addressof(value)), sizeof(value));
    }

    class TemporaryDirectory {
    public:
        TemporaryDirectory() {
            path_ = std::filesystem::temp_directory_path() / "PrismaUIPlatformTests";
            std::filesystem::remove_all(path_);
            std::filesystem::create_directories(path_);
        }

        ~TemporaryDirectory() { std::filesystem::remove_all(path_); }

        [[nodiscard]] const std::filesystem::path& Path() const noexcept { return path_; }

    private:
        std::filesystem::path path_;
    };

    bool Expect(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
        }
        return condition;
    }

    std::filesystem::path WriteSparseFixture(const std::filesystem::path& directory, std::int32_t format) {
        const auto path = directory / ("sparse-" + std::to_string(format) + ".bin");
        std::ofstream output(path, std::ios::binary);
        const std::array<std::int32_t, 4> version{1, 5, 97, 0};
        const std::int32_t nameLength = 0;
        const std::int32_t pointerSize = 8;
        const std::int32_t addressCount = 2;
        Write(output, format);
        Write(output, version);
        Write(output, nameLength);
        Write(output, pointerSize);
        Write(output, addressCount);

        const std::uint8_t absolute = 0x00;
        const std::uint64_t firstId = 10;
        const std::uint64_t firstOffset = 0x1000;
        Write(output, absolute);
        Write(output, firstId);
        Write(output, firstOffset);

        const std::uint8_t relative = 0x21;
        const std::uint8_t offsetDelta = 0x20;
        Write(output, relative);
        Write(output, offsetDelta);
        return path;
    }

    std::filesystem::path WriteDenseFixture(const std::filesystem::path& directory) {
        const auto path = directory / "dense-5.bin";
        std::ofstream output(path, std::ios::binary);
        const std::int32_t format = 5;
        const std::array<std::uint32_t, 4> version{1, 6, 1170, 0};
        const std::array<char, 64> name{};
        const std::int32_t pointerSize = 8;
        const std::int32_t dataFormat = 0;
        const std::int32_t offsetCount = 4;
        const std::array<std::uint32_t, 4> offsets{0, 0x1234, 0, 0x5678};
        Write(output, format);
        Write(output, version);
        Write(output, name);
        Write(output, pointerSize);
        Write(output, dataFormat);
        Write(output, offsetCount);
        Write(output, offsets);
        return path;
    }

    bool TestSparse(const std::filesystem::path& directory, std::int32_t format) {
        PrismaUI::Platform::AddressLibrary library;
        std::string error;
        const auto loaded = library.LoadFile(WriteSparseFixture(directory, format),
                                             PrismaUI::Platform::RuntimeFamily::kSE, {1, 5, 97, 0}, error);
        return Expect(loaded, error.c_str()) && Expect(library.Offset(10) == 0x1000, "sparse absolute lookup") &&
               Expect(library.Offset(11) == 0x1020, "sparse relative lookup") &&
               Expect(!library.Offset(12), "sparse missing ID");
    }

    bool TestDense(const std::filesystem::path& directory) {
        PrismaUI::Platform::AddressLibrary library;
        std::string error;
        const auto path = WriteDenseFixture(directory);
        if (!Expect(library.LoadFile(path, PrismaUI::Platform::RuntimeFamily::kAE, {1, 6, 1170, 0}, error),
                    error.c_str()) ||
            !Expect(library.Offset(1) == 0x1234, "dense lookup") ||
            !Expect(!library.Offset(2), "dense zero means missing")) {
            return false;
        }
        return Expect(!library.LoadFile(path, PrismaUI::Platform::RuntimeFamily::kAE, {1, 6, 640, 0}, error),
                      "dense version mismatch must fail");
    }

    bool TestCsv(const std::filesystem::path& directory) {
        const auto validPath = directory / "valid.csv";
        {
            std::ofstream output(validPath);
            output << "id,offset\n2,0.253.0\n10,001000\n11,001020\n";
        }

        PrismaUI::Platform::AddressLibrary library;
        std::string error;
        if (!Expect(library.LoadFile(validPath, PrismaUI::Platform::RuntimeFamily::kVR, {1, 4, 15, 0}, error),
                    error.c_str()) ||
            !Expect(library.Offset(10) == 0x1000, "CSV lookup")) {
            return false;
        }

        const auto duplicatePath = directory / "duplicate.csv";
        {
            std::ofstream output(duplicatePath);
            output << "id,offset\n2,0.253.0\n10,001000\n10,001020\n";
        }
        return Expect(!library.LoadFile(duplicatePath, PrismaUI::Platform::RuntimeFamily::kVR, {1, 4, 15, 0}, error),
                      "CSV duplicate ID must fail");
    }

    bool TestImpossibleCounts(const std::filesystem::path& directory) {
        const auto densePath = directory / "dense-impossible-count.bin";
        {
            std::ofstream output(densePath, std::ios::binary);
            const std::int32_t format = 5;
            const std::array<std::uint32_t, 4> version{1, 6, 1170, 0};
            const std::array<char, 64> name{};
            const std::int32_t pointerSize = 8;
            const std::int32_t dataFormat = 0;
            const std::int32_t offsetCount = (std::numeric_limits<std::int32_t>::max)();
            Write(output, format);
            Write(output, version);
            Write(output, name);
            Write(output, pointerSize);
            Write(output, dataFormat);
            Write(output, offsetCount);
        }

        PrismaUI::Platform::AddressLibrary library;
        std::string error;
        if (!Expect(!library.LoadFile(densePath, PrismaUI::Platform::RuntimeFamily::kAE, {1, 6, 1170, 0}, error),
                    "dense impossible count must fail")) {
            return false;
        }

        const auto sparsePath = directory / "sparse-impossible-count.bin";
        {
            std::ofstream output(sparsePath, std::ios::binary);
            const std::int32_t format = 2;
            const std::array<std::int32_t, 4> version{1, 5, 97, 0};
            const std::int32_t nameLength = 0;
            const std::int32_t pointerSize = 8;
            const std::int32_t addressCount = (std::numeric_limits<std::int32_t>::max)();
            Write(output, format);
            Write(output, version);
            Write(output, nameLength);
            Write(output, pointerSize);
            Write(output, addressCount);
        }
        if (!Expect(!library.LoadFile(sparsePath, PrismaUI::Platform::RuntimeFamily::kSE, {1, 5, 97, 0}, error),
                    "sparse impossible count must fail")) {
            return false;
        }

        const auto csvPath = directory / "csv-impossible-count.csv";
        {
            std::ofstream output(csvPath);
            output << "id,offset\n18446744073709551615,0.253.0\n0,0\n";
        }
        return Expect(!library.LoadFile(csvPath, PrismaUI::Platform::RuntimeFamily::kVR, {1, 4, 15, 0}, error),
                      "CSV impossible count must fail");
    }

    bool TestRuntimeProfilesAndPaths() {
        std::string error;
        const auto se = PrismaUI::Platform::RuntimeContext::Create(0x01050610, error);
        const auto ae = PrismaUI::Platform::RuntimeContext::Create(0x01064920, error);
        const auto vr = PrismaUI::Platform::RuntimeContext::Create(0x010400F0, error);
        const auto unknown = PrismaUI::Platform::RuntimeContext::Create(0x01067530, error);
        if (!Expect(se && se->Profile() == PrismaUI::Platform::AbiProfile::kSE_1_5, "SE ABI profile") ||
            !Expect(ae && ae->Profile() == PrismaUI::Platform::AbiProfile::kAE_1130, "AE ABI profile") ||
            !Expect(vr && vr->Profile() == PrismaUI::Platform::AbiProfile::kVR_1_4_15, "VR ABI profile") ||
            !Expect(!unknown, "unknown ABI profile must fail")) {
            return false;
        }

        const std::filesystem::path root = "C:/Skyrim";
        return Expect(PrismaUI::Platform::AddressLibrary::ExpectedPath(*se, root).filename() == "version-1-5-97-0.bin",
                      "SE Address Library filename") &&
               Expect(PrismaUI::Platform::AddressLibrary::ExpectedPath(*ae, root).filename() ==
                          "versionlib-1-6-1170-0.bin",
                      "AE Address Library filename") &&
               Expect(PrismaUI::Platform::AddressLibrary::ExpectedPath(*vr, root).filename() == "version-1-4-15-0.csv",
                      "VR Address Library filename");
    }

    bool TestSuppliedVrDatabase() {
        const auto path =
            std::filesystem::path(PRISMAUI_SOURCE_DIR) / "test-resources" / "addresslibrary" / "version-1-4-15-0.csv";
        PrismaUI::Platform::AddressLibrary library;
        std::string error;
        if (!Expect(library.LoadFile(path, PrismaUI::Platform::RuntimeFamily::kVR, {1, 4, 15, 0}, error),
                    error.c_str())) {
            return false;
        }
        constexpr std::array<std::uint64_t, 10> requiredIds{75461, 524907, 524998, 514178, 514705,
                                                            67245, 516574, 11045,  66859,  66860};
        for (const auto id : requiredIds) {
            if (!Expect(library.Offset(id).has_value(), "required VR relocation ID")) {
                return false;
            }
        }
        return true;
    }

    bool TestLoggingSourceLocation() {
        std::ostringstream output;
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
        auto log = std::make_shared<spdlog::logger>("source-location-test", std::move(sink));
        log->set_pattern("[%s:%#] %v");
        spdlog::set_default_logger(log);

        PrismaUI::Platform::Logging::info("source location test");
        log->flush();

        const auto text = output.str();
        return Expect(text.find("[AddressLibraryTests.cpp:") != std::string::npos, "logging source file and line");
    }
}

int main() {
    const TemporaryDirectory temporaryDirectory;
    const bool passed = TestSparse(temporaryDirectory.Path(), 1) && TestSparse(temporaryDirectory.Path(), 2) &&
                        TestDense(temporaryDirectory.Path()) && TestCsv(temporaryDirectory.Path()) &&
                        TestImpossibleCounts(temporaryDirectory.Path()) &&
                        TestRuntimeProfilesAndPaths() && TestSuppliedVrDatabase() && TestLoggingSourceLocation();
    if (passed) {
        std::cout << "All PrismaUI platform tests passed\n";
        return 0;
    }
    return 1;
}
