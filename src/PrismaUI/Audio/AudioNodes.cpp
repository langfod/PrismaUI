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

        if (inputs.empty()) {
            // [026] Update scratch so a subsequent cached-path read sees silence, not stale data
            if (scratchL.size() < numFrames) {
                scratchL.resize(numFrames);
                scratchR.resize(numFrames);
            }
            std::memset(scratchL.data(), 0, numFrames * sizeof(float));
            std::memset(scratchR.data(), 0, numFrames * sizeof(float));
            return;
        }

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
        if (!gain.hasEvents.load(std::memory_order_relaxed)) {
            // Fast path: constant gain — no mutex needed
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
    void AudioBufferSourceNode::Start(double when, double offset, double duration) {
        if (started.load(std::memory_order_relaxed)) return;  // Can only be started once (per spec)
        startTime.store(when, std::memory_order_relaxed);
        startOffset = offset;
        if (duration >= 0.0) {
            playbackDuration.store(duration, std::memory_order_relaxed);
        }
        AudioBuffer* buf = buffer.load(std::memory_order_relaxed);
        if (buf && offset > 0.0) {
            playbackPosition = static_cast<uint64_t>(offset * buf->sampleRate);
        }
        started.store(true, std::memory_order_release);  // Publish all preceding writes
    }

    void AudioBufferSourceNode::Stop(double when) {
        stopTime.store(when, std::memory_order_relaxed);
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

        AudioBuffer* buf = buffer.load(std::memory_order_acquire);

        // Not started, ended, or no buffer => silence
        if (!started.load(std::memory_order_acquire) || ended.load(std::memory_order_relaxed) || !buf || buf->length == 0) {
            std::memset(outL, 0, numFrames * sizeof(float));
            std::memset(outR, 0, numFrames * sizeof(float));
            std::memcpy(scratchL.data(), outL, numFrames * sizeof(float));
            std::memcpy(scratchR.data(), outR, numFrames * sizeof(float));
            return;
        }

        uint32_t bufferLength = buf->length;
        uint32_t numChannels = buf->numberOfChannels;
        const float* ch0 = (numChannels > 0) ? buf->channelData[0].data() : nullptr;
        const float* ch1 = (numChannels > 1) ? buf->channelData[1].data() : ch0;

        // Load atomic params once per render block to avoid repeated atomic reads in the hot loop
        const double nodeStartTime = startTime.load(std::memory_order_relaxed);
        const double nodeStopTime  = stopTime.load(std::memory_order_relaxed);
        const double nodeDuration  = playbackDuration.load(std::memory_order_relaxed);
        const bool   nodeLoop      = loop.load(std::memory_order_relaxed);
        const double nodeLoopStart = loopStart.load(std::memory_order_relaxed);
        const double nodeLoopEnd   = loopEnd.load(std::memory_order_relaxed);

        // Compute duration end sample (relative to startOffset in the buffer)
        uint64_t durationEndSample = UINT64_MAX;
        if (nodeDuration >= 0.0) {
            uint64_t offsetSample = static_cast<uint64_t>(startOffset * buf->sampleRate);
            durationEndSample = offsetSample + static_cast<uint64_t>(nodeDuration * buf->sampleRate);
        }

        // Determine loop boundaries
        uint64_t loopStartSample = 0;
        uint64_t loopEndSample = bufferLength;
        if (nodeLoop) {
            if (nodeLoopStart > 0.0) {
                loopStartSample = static_cast<uint64_t>(nodeLoopStart * buf->sampleRate);
                loopStartSample = std::min(loopStartSample, static_cast<uint64_t>(bufferLength));
            }
            if (nodeLoopEnd > 0.0) {
                loopEndSample = static_cast<uint64_t>(nodeLoopEnd * buf->sampleRate);
                loopEndSample = std::min(loopEndSample, static_cast<uint64_t>(bufferLength));
            }
            if (loopEndSample <= loopStartSample) {
                loopEndSample = bufferLength;
            }
        }

        for (uint32_t i = 0; i < numFrames; ++i) {
            double frameTime = contextTime + static_cast<double>(i) / sampleRate;

            // Not yet at start time => silence
            if (frameTime < nodeStartTime) {
                outL[i] = 0.0f;
                outR[i] = 0.0f;
                continue;
            }

            // Check stop time
            if (nodeStopTime >= 0.0 && frameTime >= nodeStopTime) {
                ended = true;
                endedEventPending = true;
                // Fill remaining with silence
                std::memset(outL + i, 0, (numFrames - i) * sizeof(float));
                std::memset(outR + i, 0, (numFrames - i) * sizeof(float));
                break;
            }

            // Check duration limit
            if (playbackPosition >= durationEndSample) {
                ended = true;
                endedEventPending = true;
                std::memset(outL + i, 0, (numFrames - i) * sizeof(float));
                std::memset(outR + i, 0, (numFrames - i) * sizeof(float));
                break;
            }

            // Check bounds
            if (playbackPosition >= bufferLength) {
                if (nodeLoop) {
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
            if (nodeLoop && playbackPosition >= loopEndSample) {
                playbackPosition = loopStartSample;
            }
        }

        // Cache
        std::memcpy(scratchL.data(), outL, numFrames * sizeof(float));
        std::memcpy(scratchR.data(), outR, numFrames * sizeof(float));
    }

}  // namespace PrismaUI::Audio
