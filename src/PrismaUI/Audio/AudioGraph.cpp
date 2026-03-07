#include "AudioGraph.h"

#include <algorithm>

namespace PrismaUI::Audio {

    void ConnectNodes(AudioNode* source, AudioNode* destination) {
        if (!source || !destination) return;

        // [020] Guard against self-connections which would create infinite render cycles
        if (source == destination) return;

        // Prevent duplicate connections
        auto it = std::find(destination->inputs.begin(), destination->inputs.end(), source);
        if (it != destination->inputs.end()) return;

        destination->inputs.push_back(source);
        source->outputs.push_back(destination);
    }

    void DisconnectNodes(AudioNode* source, AudioNode* destination) {
        if (!source || !destination) return;

        auto it = std::find(destination->inputs.begin(), destination->inputs.end(), source);
        if (it != destination->inputs.end()) {
            destination->inputs.erase(it);
        }

        auto it2 = std::find(source->outputs.begin(), source->outputs.end(), destination);
        if (it2 != source->outputs.end()) {
            source->outputs.erase(it2);
        }
    }

    void DisconnectNode(AudioNode* node) {
        if (!node) return;

        // Remove this node from all its outputs' input lists
        for (auto* output : node->outputs) {
            auto it = std::find(output->inputs.begin(), output->inputs.end(), node);
            if (it != output->inputs.end()) {
                output->inputs.erase(it);
            }
        }

        // Remove this node from all its inputs' output lists
        for (auto* input : node->inputs) {
            auto it = std::find(input->outputs.begin(), input->outputs.end(), node);
            if (it != input->outputs.end()) {
                input->outputs.erase(it);
            }
        }

        node->inputs.clear();
        node->outputs.clear();
    }

}  // namespace PrismaUI::Audio
