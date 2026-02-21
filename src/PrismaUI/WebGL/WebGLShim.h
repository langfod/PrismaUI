#pragma once

#include <string>

namespace PrismaUI::WebGL {

    // Returns the JavaScript shim code that overrides canvas.getContext('webgl')
    // to return a PrismaUI-backed WebGL context. Must be injected before page scripts.
    const std::string& GetShimJS();

}  // namespace PrismaUI::WebGL
