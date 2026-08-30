#pragma once

#include <spdlog/spdlog.h>

#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

#include "Platform/RuntimeContext.h"


namespace PrismaUI::Platform::Logging {
    namespace detail {
        template <class... Args>
        struct FormatWithLocation {
            template <class T>
            consteval FormatWithLocation(const T& format,
                                         std::source_location location = std::source_location::current())
                : format(format), location(location) {}

            spdlog::format_string_t<Args...> format;
            std::source_location location;
        };

        template <spdlog::level::level_enum Level, class... Args>
        void Log(FormatWithLocation<std::type_identity_t<Args>...> format, Args&&... args) {
            const auto& location = format.location;
            spdlog::default_logger_raw()->log(
                spdlog::source_loc{location.file_name(), static_cast<int>(location.line()), location.function_name()},
                Level, format.format, std::forward<Args>(args)...);
        }
    }

    template <class... Args>
    void trace(detail::FormatWithLocation<std::type_identity_t<Args>...> format, Args&&... args) {
        detail::Log<spdlog::level::trace>(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void debug(detail::FormatWithLocation<std::type_identity_t<Args>...> format, Args&&... args) {
        detail::Log<spdlog::level::debug>(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void info(detail::FormatWithLocation<std::type_identity_t<Args>...> format, Args&&... args) {
        detail::Log<spdlog::level::info>(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void warn(detail::FormatWithLocation<std::type_identity_t<Args>...> format, Args&&... args) {
        detail::Log<spdlog::level::warn>(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void error(detail::FormatWithLocation<std::type_identity_t<Args>...> format, Args&&... args) {
        detail::Log<spdlog::level::err>(format, std::forward<Args>(args)...);
    }

    template <class... Args>
    void critical(detail::FormatWithLocation<std::type_identity_t<Args>...> format, Args&&... args) {
        detail::Log<spdlog::level::critical>(format, std::forward<Args>(args)...);
    }

    [[nodiscard]] bool Initialize(RuntimeFamily runtime, std::string& error) noexcept;
}
