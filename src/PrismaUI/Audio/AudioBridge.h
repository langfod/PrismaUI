#pragma once

#include <JavaScriptCore/JavaScript.h>

#include <cstdint>

namespace PrismaUI::Audio {

    struct AudioContext;

    // Initialize Web Audio JSC bindings for a view. Injects the __prismaCreateAudioContext
    // native function onto the global JS object. Called from OnWindowObjectReady.
    void InjectAudioBindings(JSContextRef jsCtx, uint64_t viewId);

    // Destroy an AudioContext (stops device, frees resources). Safe to call with nullptr.
    void DestroyAudioContext(AudioContext* ctx);

    // Get JSClassRef for each audio type (created once, shared)
    JSClassRef GetAudioContextClass();
    JSClassRef GetAudioDestinationNodeClass();
    JSClassRef GetGainNodeClass();
    JSClassRef GetBufferSourceNodeClass();
    JSClassRef GetAudioBufferClass();
    JSClassRef GetAudioParamClass();

}  // namespace PrismaUI::Audio
