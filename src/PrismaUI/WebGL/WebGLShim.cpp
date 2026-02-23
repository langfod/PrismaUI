#include "WebGLShim.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "Utils/DllLoader.h"

namespace PrismaUI::WebGL {

    const std::string& GetShimJS() {
        static std::string cachedShimJS;
        static bool loaded = false;

        if (!loaded) {
            loaded = true;
            auto shimPath = Utils::GetBasePath() / "misc" / "webgl_shim.js";
            std::ifstream file(shimPath, std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::ostringstream ss;
                ss << file.rdbuf();
                cachedShimJS = ss.str();
                logger::info("[WebGL] Loaded shim JS from: {} ({} bytes)", shimPath.string(), cachedShimJS.size());
            } else {
                logger::error("[WebGL] Failed to load shim JS from: {}", shimPath.string());
            }
        }

        return cachedShimJS;
    }

}  // namespace PrismaUI::WebGL
