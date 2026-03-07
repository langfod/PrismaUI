#include "AudioNodes.h"
#include "AudioBuffer.h"
#include "AudioContext.h"

#include <algorithm>
#include <cstring>

namespace PrismaUI::Audio {

    // ---- AudioDestinationNode ----
    void AudioDestinationNode::Process(float* outL, float* outR, uint32_t numFrames,
                                        double contextTime, float sampleRate) {
        if (lastRenderFrame == context->renderFrame) {
            // Already rendered this frame — return cached scratch
            std::memcpy(outL, scratchL.data(), numFrames * sizeof(float));
            std::memcpy(outR, scratchR.data(), numFrames * sizeof(float));
            return;
        }
        lastRenderFrame = context->renderFrame;

        std::memset(outL, 0, numFrames * sizeof(float));
        std::memset(outR, 0, numFrames * sizeof(float));

        if (inputs.empty()) return;

        // Ensure scratch buffers are large enough
        if (scratchL.size() < numFrames) {
            scratchL.resize(numFrames);
            scratchR.resize(numFrames);
        }

        for (auto* input : inputs) {
            std::memset(scratchL.data(), 0, numFrames * sizeof(float));
            std::memset(scratchR.data(), 0, numFrames * sizeof(float));

            input->Process(scratchL.data(), scratchR.data(), numFrames, contextTime, sampleRate);

            for (uint32_t i = 0; i < numFrames; ++i) {
                outL[i] += scratchL[i];
                outR[i] += scratchR[i];
            }
        }

        // Cache for cycle detection
        std::memcpy(scratchL.data(), outL, numFrames * sizeof(float));
        std::memcpy(scratchR.data(), outR, numFrames * sizeof(float));
    }

    // ---- GainNode ----
    void GainNode::Process(float* outL, float* outR, uint32_t numFrames,
                           double contextTime, float sampleRate) {
        if (lastRenderFrame == context->renderFrame) {
            std::memcpy(outL, scratchL.data(), numFrames * sizeof(float));
            std::memcpy(outR, scratchR.data(), numFrames * sizeof(float));
            return;
        }
        lastRenderFrame = context->renderFrame;

        std::memset(outL, 0, numFrames * sizeof(float));
        std::memset(outR, 0, numFrames * sizeof(float));

        if (scratchL.size() < numFrames) {
            scratchL.resize(numFrames);
            scratchR.resize(numFrames);
        }

        // Pull from inputs and sum
        for (auto* input : inputs) {
            std::memset(scratchL.data(), 0, numFrames * sizeof(float));
            std::memset(scratchR.data(), 0, numFrames * sizeof(float));

            input->Process(scratchL.data(), scratchR.data(), numFrames, contextTime, sampleRate);

            for (uint32_t i = 0; i < numFrames; ++i) {
                outL[i] += scratchL[i];
                outR[i] += scratchR[i];
            }
        }

        // Apply gain
        if (gain.events.empty()) {
            // Fast path: constant gain
            float g = gain.value.load(std::memory_order_relaxed);
            for (uint32_t i = 0; i < numFrames; ++i) {
                outL[i] *= g;
                outR[i] *= g;
            }
        } else {
            // Per-sample gain from automation
            for (uint32_t i = 0; i < numFrames; ++i) {
                double t = contextTime + static_cast<double>(i) / sampleRate;
                float g = gain.Evaluate(t);
                outL[i] *= g;
                outR[i] *= g;
            }
        }

        // Cache
        std::memcpy(scratchL.data(), outL, numFrames * sizeof(float));
        std::memcpy(scratchR.data(), outR, numFrames * sizeof(float));
    }

    // ---- AudioBufferSourceNode ----
    void AudioBufferSourceNode::Start(double when, double offset, double /*duration*/) {
        if (started.load(std::memory_order_relaxed)) return;  // Can only be started once (per spec)
        startTime = when;
        startOffset = offset;
        if (buffer && offset > 0.0) {
            playbackPosition = static_cast<uint64_t>(offset * buffer->sampleRate);
        }
        started.store(true, std::memory_order_release);  // Publish all preceding writes
    }

    void AudioBufferSourceNode::Stop(double when) {
        stopTime = when;
    }

    void AudioBufferSourceNode::Process(float* outL, float* outR, uint32_t numFrames,
                                         double contextTime, float sampleRate) {
        if (lastRenderFrame == context->renderFrame) {
            std::memcpy(outL, scratchL.data(), numFrames * sizeof(float));
            std::memcpy(outR, scratchR.data(), numFrames * sizeof(float));
            return;
        }
        lastRenderFrame = context->renderFrame;

        if (scratchL.size() < numFrames) {
            scratchL.resize(numFrames);
            scratchR.resize(numFrames);
        }

        // Not started, ended, or no buffer => silence
        if (!started.load(std::memory_order_acquire) || ended.load(std::memory_order_relaxed) || !buffer || buffer->length == 0) {
            std::memset(outL, 0, numFrames * sizeof(float));
            std::memset(outR, 0, numFrames * sizeof(float));
            std::memcpy(scratchL.data(), outL, numFrames * sizeof(float));
            std::memcpy(scratchR.data(), outR, numFrames * sizeof(float));
            return;
        }

        uint32_t bufferLength = buffer->length;
        uint32_t numChannels = buffer->numberOfChannels;
        const float* ch0 = (numChannels > 0) ? buffer->channelData[0].data() : nullptr;
        const float* ch1 = (numChannels > 1) ? buffer->channelData[1].data() : ch0;

        // Determine loop boundaries
        uint64_t loopStartSample = 0;
        uint64_t loopEndSample = bufferLength;
        if (loop) {
            if (loopStart > 0.0) {
                loopStartSample = static_cast<uint64_t>(loopStart * buffer->sampleRate);
                loopStartSample = std::min(loopStartSample, static_cast<uint64_t>(bufferLength));
            }
            if (loopEnd > 0.0) {
                loopEndSample = static_cast<uint64_t>(loopEnd * buffer->sampleRate);
                loopEndSample = std::min(loopEndSample, static_cast<uint64_t>(bufferLength));
            }
            if (loopEndSample <= loopStartSample) {
                loopEndSample = bufferLength;
            }
        }

        for (uint32_t i = 0; i < numFrames; ++i) {
            double frameTime = contextTime + static_cast<double>(i) / sampleRate;

            // Not yet at start time => silence
            if (frameTime < startTime) {
                outL[i] = 0.0f;
                outR[i] = 0.0f;
                continue;
            }

            // Check stop time
            if (stopTime >= 0.0 && frameTime >= stopTime) {
                ended = true;
                endedEventPending = true;
                // Fill remaining with silence
                std::memset(outL + i, 0, (numFrames - i) * sizeof(float));
                std::memset(outR + i, 0, (numFrames - i) * sizeof(float));
                break;
            }

            // Check bounds
            if (playbackPosition >= bufferLength) {
                if (loop) {
                    playbackPosition = loopStartSample;
                } else {
                    ended = true;
                    endedEventPending = true;
                    std::memset(outL + i, 0, (numFrames - i) * sizeof(float));
                    std::memset(outR + i, 0, (numFrames - i) * sizeof(float));
                    break;
                }
            }

            // Read sample
            if (ch0) outL[i] = ch0[playbackPosition];
            else outL[i] = 0.0f;

            if (ch1) outR[i] = ch1[playbackPosition];
            else outR[i] = outL[i];  // Mono upmix

            playbackPosition++;

            // Loop wrap
            if (loop && playbackPosition >= loopEndSample) {
                playbackPosition = loopStartSample;
            }
        }

        // Cache
        std::memcpy(scratchL.data(), outL, numFrames * sizeof(float));
        std::memcpy(scratchR.data(), outR, numFrames * sizeof(float));
    }

}  // namespace PrismaUI::Audio
