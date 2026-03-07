#include "WebGLShim.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "Utils/DllLoader.h"

namespace PrismaUI::WebGL {

    const std::string& GetShimJS() {
        static const std::string s = []() -> std::string {
            auto shimPath = Utils::GetBasePath() / "misc" / "webgl_shim.js";
            std::ifstream file(shimPath, std::ios::in | std::ios::binary);
            if (file.is_open()) {
                std::ostringstream ss;
                ss << file.rdbuf();
                std::string result = ss.str();
                logger::info("[WebGL] Loaded shim JS from: {} ({} bytes)", shimPath.string(), result.size());
                return result;
            }
            logger::error("[WebGL] Failed to load shim JS from: {}", shimPath.string());
            return {};
        }();
        return s;
    }

}  // namespace PrismaUI::WebGL
