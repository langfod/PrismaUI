#include "Platform/Logging.h"

#include <ShlObj.h>
#include <Windows.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <filesystem>
#include <memory>

#ifndef NDEBUG
    #include <spdlog/sinks/msvc_sink.h>
#endif

namespace PrismaUI::Platform::Logging {
    bool Initialize(RuntimeFamily runtime, std::string& error) noexcept {
        try {
            PWSTR documentsPath = nullptr;
            const auto result = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documentsPath);
            if (FAILED(result) || !documentsPath) {
                error = "Unable to locate the Documents directory for the PrismaUI log";
                return false;
            }

            std::filesystem::path logDirectory(documentsPath);
            CoTaskMemFree(documentsPath);
            logDirectory /= "My Games";
            logDirectory /= runtime == RuntimeFamily::kVR ? "Skyrim VR" : "Skyrim Special Edition";
            logDirectory /= "SKSE";
            std::filesystem::create_directories(logDirectory);

            std::vector<spdlog::sink_ptr> sinks;
            sinks.push_back(
                std::make_shared<spdlog::sinks::basic_file_sink_mt>((logDirectory / "PrismaUI.log").string(), true));
#ifndef NDEBUG
            sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif
            auto log = std::make_shared<spdlog::logger>("PrismaUI", sinks.begin(), sinks.end());
            spdlog::set_default_logger(std::move(log));
            spdlog::set_pattern("[%Y-%m-%d %T.%e] [%l] [%t] [%s:%#] %v");
            spdlog::set_level(spdlog::level::trace);
            spdlog::flush_on(spdlog::level::debug);
            error.clear();
            return true;
        } catch (const std::exception& exception) {
            error = exception.what();
            return false;
        } catch (...) {
            error = "Unknown error while initializing the PrismaUI log";
            return false;
        }
    }
}
