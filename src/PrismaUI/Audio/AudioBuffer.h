#pragma once

#include <cstdint>
#include <vector>

namespace PrismaUI::Audio {

    struct AudioBuffer {
        uint32_t numberOfChannels = 0;
        uint32_t length = 0;       // Number of sample frames per channel
        float sampleRate = 0.0f;
        double duration = 0.0;     // length / sampleRate

        // Per-channel sample data (non-interleaved float32)
        std::vector<std::vector<float>> channelData;
    };

    // Create an empty AudioBuffer with allocated (zeroed) channel data.
    AudioBuffer* CreateBuffer(uint32_t numChannels, uint32_t length, float sampleRate);

    // Decode audio data from memory (WAV, MP3, FLAC, OGG) into an AudioBuffer.
    // Resamples to targetSampleRate if needed. Returns nullptr on failure.
    AudioBuffer* DecodeFromMemory(const uint8_t* data, size_t dataSize, float targetSampleRate);

}  // namespace PrismaUI::Audio
