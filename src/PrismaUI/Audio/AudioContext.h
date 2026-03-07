#pragma once

#include "AudioCommandQueue.h"
#include "AudioGraph.h"

#include <xaudio2.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace PrismaUI::Audio {

    struct AudioBuffer;
    struct AudioContext;
    struct AudioDestinationNode;

    enum class AudioContextState : uint8_t {
        Suspended,
        Running,
        Closed
    };

    // XAudio2 voice callback + double-buffer output stage.
    struct XAudio2Output : IXAudio2VoiceCallback {
        AudioContext* ctx = nullptr;
        IXAudio2SourceVoice* sourceVoice = nullptr;

        static constexpr uint32_t kBufferFrames = 256;
        static constexpr uint32_t kChannels = 2;
        static constexpr uint32_t kBufferBytes = kBufferFrames * kChannels * sizeof(float);
        float buffers[2][kBufferFrames * kChannels]{};
        uint32_t currentBuffer = 0;

        // IXAudio2VoiceCallback
        void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
        void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
        void STDMETHODCALLTYPE OnStreamEnd() override {}
        void STDMETHODCALLTYPE OnBufferStart(void*) override {}
        void STDMETHODCALLTYPE OnBufferEnd(void* pBufferContext) override;
        void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
        void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT error) override;
    };

    struct AudioContext {
        XAudio2Output xaOutput;                        // XAudio2 voice + double-buffer
        IXAudio2* xaEngine = nullptr;                  // Owned — created via XAudio2Create()
        IXAudio2MasteringVoice* masterVoice = nullptr; // Owned — routes to default output device

        std::atomic<AudioContextState> state{AudioContextState::Suspended};
        std::atomic<bool> destroying{false};
        float sampleRate = 48000.0f;
        std::atomic<double> currentTime_{0.0};  // Written by audio thread, read by JS thread
        uint64_t renderFrame = 0;   // Incremented each render batch for cycle detection

        // Approximate count of orphaned nodes awaiting JS-thread collection.
        // Incremented by audio thread (sweep), reset by JS thread (collect).
        std::atomic<uint32_t> orphanedNodeCount{0};

        // Lock-free command queue for graph topology changes.
        // JS thread: TryPush() to enqueue connect/disconnect commands.
        // Audio thread: drains queue at the start of each callback.
        AudioCommandQueue commandQueue_;

        // All nodes owned by this context. Destroyed when the context is destroyed.
        std::vector<std::unique_ptr<AudioNode>> nodes;

        // All buffers owned by this context. Prevents GC use-after-free — buffers
        // live until the context is destroyed, not when JS drops its reference.
        std::vector<std::unique_ptr<AudioBuffer>> buffers;

        // The output sink — always the first node created, never null while context lives.
        AudioDestinationNode* destinationNode = nullptr;

        std::atomic<bool> destroyed{false};
    };

    // Create a new AudioContext with a standalone XAudio2 engine and mastering voice.
    // Starts in Suspended state. Returns nullptr on failure.
    AudioContext* CreateAudioContext(float requestedSampleRate = 0.0f);

    // Resume playback (starts the XAudio2 source voice).
    void ResumeAudioContext(AudioContext* ctx);

    // Suspend playback (stops the XAudio2 source voice).
    void SuspendAudioContext(AudioContext* ctx);

    // Destroy the context: stops voice, releases engine, deletes all nodes, frees memory.
    void DestroyAudioContext(AudioContext* ctx);

    // Reclaim memory from orphaned nodes and their buffers.
    // Called from JS thread only (during node/buffer creation).
    void CollectDeadNodes(AudioContext* ctx);

}  // namespace PrismaUI::Audio
