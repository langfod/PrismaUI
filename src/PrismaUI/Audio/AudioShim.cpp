#include "AudioShim.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "Utils/DllLoader.h"

namespace PrismaUI::Audio {

    const std::string& GetAudioShimJS() {
        static std::string cachedShimJS;
        static bool loaded = false;

        if (!loaded) {
            loaded = true;
            auto shimPath = Utils::GetBasePath() / "misc" / "webaudio_shim.js";
            std::ifstream file(shimPath, std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::ostringstream ss;
                ss << file.rdbuf();
                cachedShimJS = ss.str();
                logger::info("[Audio] Loaded audio shim JS from: {} ({} bytes)",
                             shimPath.string(), cachedShimJS.size());
            } else {
                logger::error("[Audio] Failed to load audio shim JS from: {}", shimPath.string());
            }
        }

        return cachedShimJS;
    }

}  // namespace PrismaUI::Audio
