#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace PrismaUI::Audio {

    struct AudioNode;

    // A command representing a deferred graph mutation.
    // Produced by the JS thread, consumed by the audio callback thread.
    struct AudioCommand {
        enum class Type : uint8_t {
            Connect,       // Connect nodeA's output to nodeB's input
            Disconnect,    // Disconnect nodeA's output from nodeB's input
            DisconnectAll  // Disconnect all of nodeA's outputs
        };

        Type type = Type::Connect;
        AudioNode* nodeA = nullptr;  // Source (Connect/Disconnect) or node (DisconnectAll)
        AudioNode* nodeB = nullptr;  // Destination (nullptr for DisconnectAll)
    };

    // Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
    //
    // Producer: ultralightThread (JS bridge calls)
    // Consumer: XAudio2 OnBufferEnd callback thread
    struct AudioCommandQueue {
        static constexpr size_t kCapacity = 256;

        // Returns true if the command was enqueued, false if the queue is full.
        bool TryPush(AudioCommand cmd) {
            size_t w = writePos_.load(std::memory_order_relaxed);
            size_t nextW = (w + 1) % kCapacity;

            if (nextW == readPos_.load(std::memory_order_acquire)) {
                return false;
            }

            commands_[w] = cmd;
            writePos_.store(nextW, std::memory_order_release);
            return true;
        }

        // Returns true if a command was dequeued into `out`, false if empty.
        bool TryPop(AudioCommand& out) {
            size_t r = readPos_.load(std::memory_order_relaxed);

            // Empty if read position equals write position
            if (r == writePos_.load(std::memory_order_acquire)) {
                return false;
            }

            out = commands_[r];
            readPos_.store((r + 1) % kCapacity, std::memory_order_release);
            return true;
        }

    private:
        AudioCommand commands_[kCapacity]{};
        std::atomic<size_t> writePos_{0};
        std::atomic<size_t> readPos_{0};
    };

}  // namespace PrismaUI::Audio
