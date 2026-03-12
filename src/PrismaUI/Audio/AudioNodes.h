#pragma once

#include <atomic>
#include <cstdint>

#include "AudioGraph.h"
#include "AudioParam.h"


namespace PrismaUI::Audio {

    struct AudioBuffer;

    // ---- AudioDestinationNode ----
    struct AudioDestinationNode : AudioNode {
        uint32_t maxChannelCount = 2;

        AudioDestinationNode() { type = Type::Destination; }

        void Process(float* outL, float* outR, uint32_t numFrames, double contextTime, float sampleRate) override;
    };

    // ---- GainNode ----
    struct GainNode : AudioNode {
        AudioParam gain{1.0f};

        GainNode() { type = Type::Gain; }

        void Process(float* outL, float* outR, uint32_t numFrames, double contextTime, float sampleRate) override;
    };

    // ---- AudioBufferSourceNode ----
    struct AudioBufferSourceNode : AudioNode {
        std::atomic<AudioBuffer*> buffer{nullptr};  // JS-thread write, audio-thread read
        std::atomic<bool> loop{false};
        std::atomic<double> loopStart{0.0};
        std::atomic<double> loopEnd{0.0};
        AudioParam playbackRate{1.0f};

        std::atomic<bool> started{false};
        std::atomic<bool> ended{false};
        std::atomic<double> startTime{0.0};
        std::atomic<double> stopTime{-1.0};          // Negative means no stop scheduled
        std::atomic<double> playbackDuration{-1.0};  // Negative means no duration limit
        double startOffset = 0.0;                    // Only used in Start(), guarded by started fence
        uint64_t playbackPosition = 0;               // Audio-thread-only after Start()

        AudioBufferSourceNode() { type = Type::BufferSource; }

        void Start(double when = 0.0, double offset = 0.0, double duration = -1.0);
        void Stop(double when = 0.0);

        void Process(float* outL, float* outR, uint32_t numFrames, double contextTime, float sampleRate) override;

        // Set by the JS bridge; called from the audio thread when playback ends.
        // The flag is polled by a JS-side update mechanism to fire 'onended'.
        std::atomic<bool> endedEventPending{false};
    };

}  // namespace PrismaUI::Audio
