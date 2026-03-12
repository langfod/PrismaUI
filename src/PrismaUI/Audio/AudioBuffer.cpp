#include "AudioBuffer.h"

#include <miniaudio.h>

#include <cstring>


namespace logger = SKSE::log;

namespace PrismaUI::Audio {

    std::unique_ptr<AudioBuffer> CreateBuffer(uint32_t numChannels, uint32_t length, float sampleRate) {
        if (numChannels == 0 || length == 0 || sampleRate <= 0.0f) return nullptr;

        auto buf = std::make_unique<AudioBuffer>();
        buf->numberOfChannels = numChannels;
        buf->length = length;
        buf->sampleRate = sampleRate;
        buf->duration = static_cast<double>(length) / sampleRate;

        buf->channelData.resize(numChannels);
        for (uint32_t ch = 0; ch < numChannels; ++ch) {
            buf->channelData[ch].resize(length, 0.0f);
        }

        return buf;
    }

    std::unique_ptr<AudioBuffer> DecodeFromMemory(const uint8_t* data, size_t dataSize, float targetSampleRate) {
        if (!data || dataSize == 0 || targetSampleRate <= 0.0f) return nullptr;

        ma_decoder decoder;
        ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32,
                                                                 0,  // 0 = preserve source channel count
                                                                 static_cast<ma_uint32>(targetSampleRate));

        ma_result result = ma_decoder_init_memory(data, dataSize, &decoderConfig, &decoder);
        if (result != MA_SUCCESS) {
            logger::error("[Audio] Failed to init decoder from memory: {}", static_cast<int>(result));
            return nullptr;
        }

        struct DecoderGuard {
            ma_decoder* d;
            ~DecoderGuard() {
                if (d) ma_decoder_uninit(d);
            }
        } guard{&decoder};

        ma_uint64 totalFrames = 0;
        result = ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);
        if (result != MA_SUCCESS || totalFrames == 0) {
            // Length unknown — decode in chunks
            totalFrames = 0;
        }

        uint32_t channels = decoder.outputChannels;
        if (channels == 0) channels = 2;

        // Read all frames
        std::vector<float> interleavedData;
        constexpr size_t kChunkFrames = 4096;
        std::vector<float> chunk(kChunkFrames * channels);

        while (true) {
            ma_uint64 framesRead = 0;
            result = ma_decoder_read_pcm_frames(&decoder, chunk.data(), kChunkFrames, &framesRead);
            if (framesRead == 0) break;

            size_t samplesRead = static_cast<size_t>(framesRead) * channels;
            interleavedData.insert(interleavedData.end(), chunk.begin(), chunk.begin() + samplesRead);

            if (result != MA_SUCCESS) break;
        }

        if (interleavedData.empty()) {
            logger::error("[Audio] Decoded zero frames from audio data");
            return nullptr;
        }

        totalFrames = interleavedData.size() / channels;

        if (totalFrames > UINT32_MAX) {
            logger::error("[Audio] Audio too large: {} frames exceeds uint32_t", totalFrames);
            return nullptr;
        }

        // De-interleave into per-channel vectors
        auto buf = std::make_unique<AudioBuffer>();
        buf->numberOfChannels = channels;
        buf->length = static_cast<uint32_t>(totalFrames);
        buf->sampleRate = targetSampleRate;
        buf->duration = static_cast<double>(totalFrames) / targetSampleRate;
        buf->channelData.resize(channels);

        for (uint32_t ch = 0; ch < channels; ++ch) {
            buf->channelData[ch].resize(static_cast<size_t>(totalFrames));
            for (uint64_t i = 0; i < totalFrames; ++i) {
                buf->channelData[ch][static_cast<size_t>(i)] = interleavedData[static_cast<size_t>(i * channels + ch)];
            }
        }

        logger::info("[Audio] Decoded audio: {}ch, {} frames, {}Hz, {:.2f}s", channels, totalFrames, targetSampleRate,
                     buf->duration);

        return buf;
    }

}  // namespace PrismaUI::Audio
