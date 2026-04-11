#pragma once

#include <JavaScriptCore/JavaScript.h>

#include <cstdint>

namespace PrismaUI::Audio {

    struct AudioContext;

    // Initialize Web Audio JSC bindings for a view. Injects the __prismaCreateAudioContext
    // native function onto the global JS object. Called from OnWindowObjectReady.
    void InjectAudioBindings(JSContextRef jsCtx, uint64_t viewId);

    void DestroyAudioContext(AudioContext* ctx);

    JSClassRef GetAudioContextClass();
    JSClassRef GetAudioDestinationNodeClass();
    JSClassRef GetGainNodeClass();
    JSClassRef GetBufferSourceNodeClass();
    JSClassRef GetAudioBufferClass();
    JSClassRef GetAudioParamClass();

}  // namespace PrismaUI::Audio
