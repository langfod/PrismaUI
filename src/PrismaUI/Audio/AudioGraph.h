#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace PrismaUI::Audio {

    struct AudioContext;

    struct AudioNode {
        enum class Type {
            Destination,
            Gain,
            BufferSource
        };

        Type type;
        AudioContext* context = nullptr;
        uint32_t channelCount = 2;

        // Nodes whose output feeds into this node's input
        std::vector<AudioNode*> inputs;
        // Nodes this node's output feeds into
        std::vector<AudioNode*> outputs;

        // Per-node scratch buffers for rendering (avoids allocation in callback)
        std::vector<float> scratchL;
        std::vector<float> scratchR;

        // Rendering flag to prevent double-processing in diamond graphs
        uint64_t lastRenderFrame = 0;

        // Set by audio thread when node is disconnected during dead-node sweep.
        // Read by JS thread to know when it's safe to reclaim heavy resources.
        std::atomic<bool> graphOrphaned{false};

        virtual void Process(float* outL, float* outR, uint32_t numFrames,
                             double contextTime, float sampleRate) = 0;
        virtual ~AudioNode() = default;
    };

    // Connect source's output to destination's input. Call under graphMutex_.
    void ConnectNodes(AudioNode* source, AudioNode* destination);

    // Remove all connections involving this node. Call under graphMutex_.
    void DisconnectNode(AudioNode* node);

    // Disconnect a specific source->destination connection. Call under graphMutex_.
    void DisconnectNodes(AudioNode* source, AudioNode* destination);

}  // namespace PrismaUI::Audio
