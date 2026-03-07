#pragma once

#include <string>

namespace PrismaUI::Audio {

    // Returns the JavaScript shim code that overrides AudioContext and Audio
    // to use the PrismaUI native audio bridge. Must be injected before page scripts.
    const std::string& GetAudioShimJS();

}  // namespace PrismaUI::Audio
