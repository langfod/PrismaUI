#pragma once

namespace PrismaUI::Stubs {

    // Returns JavaScript that stubs the Web Audio API (AudioContext, Audio element, etc.)
    // as no-ops. Allows Emscripten and other code that probes for audio support to
    // initialise without crashing while producing no actual sound output.
    const char* GetWebAudioStubJS();

}  // namespace PrismaUI::Stubs
