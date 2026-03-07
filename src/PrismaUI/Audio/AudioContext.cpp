#include "AudioContext.h"
#include "AudioBuffer.h"
#include "AudioNodes.h"

#include <xaudio2.h>

#include <algorithm>
#include <cstring>

namespace logger = SKSE::log;

namespace PrismaUI::Audio {

    // Walk the audio graph from a node and disconnect any ended BufferSourceNodes
    // from input/output lists.  Runs on audio thread only — inputs/outputs are
    // exclusively mutated here (via command-queue drain + this sweep).
    static void SweepEndedSources(AudioContext* ctx, AudioNode* node, uint32_t depth = 0) {
        if (!node || depth > 16) return;

        auto& ins = node->inputs;
        for (auto it = ins.begin(); it != ins.end(); ) {
            AudioNode* input = *it;

            if (input->type == AudioNode::Type::BufferSource) {
                auto* src = static_cast<AudioBufferSourceNode*>(input);
                if (src->ended.load(std::memory_order_relaxed)) {
                    // Remove the edge: input -> node
                    auto outIt = std::find(input->outputs.begin(), input->outputs.end(), node);
                    if (outIt != input->outputs.end()) input->outputs.erase(outIt);
                    it = ins.erase(it);
                    input->graphOrphaned.store(true, std::memory_order_release);
                    ctx->orphanedNodeCount.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
            } else if (input->type != AudioNode::Type::Destination) {
                // Recurse into intermediate nodes (GainNode, etc.)
                SweepEndedSources(ctx, input, depth + 1);
            }

            ++it;
        }
    }

    // Render the Web Audio graph into the next double-buffer and submit to XAudio2.
    // Called from OnBufferEnd (audio thread) and during initial prime (ultralightThread).
    static void RenderAndSubmit(AudioContext* ctx) {
        auto& xa = ctx->xaOutput;
        float* buf = xa.buffers[xa.currentBuffer];

        // Drain the command queue — apply any pending graph mutations from the JS thread.
        AudioCommand cmd;
        while (ctx->commandQueue_.TryPop(cmd)) {
            switch (cmd.type) {
                case AudioCommand::Type::Connect:
                    ConnectNodes(cmd.nodeA, cmd.nodeB);
                    break;
                case AudioCommand::Type::Disconnect:
                    DisconnectNodes(cmd.nodeA, cmd.nodeB);
                    break;
                case AudioCommand::Type::DisconnectAll:
                    DisconnectNode(cmd.nodeA);
                    break;
            }
        }

        constexpr uint32_t frames = XAudio2Output::kBufferFrames;
        float scratchL[frames];
        float scratchR[frames];
        std::memset(scratchL, 0, sizeof(scratchL));
        std::memset(scratchR, 0, sizeof(scratchR));

        ctx->renderFrame++;

        if (ctx->destinationNode) {
            ctx->destinationNode->Process(scratchL, scratchR,
                frames, ctx->currentTime_.load(std::memory_order_relaxed), ctx->sampleRate);

            // Disconnect ended BufferSourceNodes from the graph so they stop
            // being iterated on every render.  This keeps Process() O(active nodes).
            SweepEndedSources(ctx, ctx->destinationNode);
        }

        // Interleave into XAudio2 buffer (L, R, L, R, ...)
        for (uint32_t i = 0; i < frames; ++i) {
            buf[i * 2]     = scratchL[i];
            buf[i * 2 + 1] = scratchR[i];
        }

        double t = ctx->currentTime_.load(std::memory_order_relaxed);
        t += static_cast<double>(frames) / ctx->sampleRate;
        ctx->currentTime_.store(t, std::memory_order_release);

        // Submit to XAudio2
        XAUDIO2_BUFFER xaBuf{};
        xaBuf.AudioBytes = XAudio2Output::kBufferBytes;
        xaBuf.pAudioData = reinterpret_cast<const BYTE*>(buf);
        xaBuf.pContext = ctx;
        // [014] Check HRESULT from SubmitSourceBuffer
        HRESULT hr = xa.sourceVoice->SubmitSourceBuffer(&xaBuf);
        if (FAILED(hr)) {
            logger::error("[Audio] SubmitSourceBuffer failed: {:08X}", static_cast<uint32_t>(hr));
        }

        xa.currentBuffer ^= 1;  // Toggle 0/1
    }

    void XAudio2Output::OnBufferEnd(void* pBufferContext) {
        auto* context = static_cast<AudioContext*>(pBufferContext);
        // [016] Check destroying flag before touching any context members.
        // Set before Stop/Flush in DestroyAudioContext; DestroyVoice() then blocks
        // until any in-flight callback has returned.
        if (!context || context->destroying.load(std::memory_order_acquire)) return;
        if (context->state.load(std::memory_order_acquire) != AudioContextState::Running) return;
        RenderAndSubmit(context);
    }

    void XAudio2Output::OnVoiceError(void* /*pBufferContext*/, HRESULT error) {
        logger::error("[Audio] XAudio2 voice error: 0x{:X}", static_cast<uint32_t>(error));
    }

    AudioContext* CreateAudioContext(float requestedSampleRate) {
        auto* ctx = new AudioContext();
        ctx->sampleRate = (requestedSampleRate > 0.0f) ? requestedSampleRate : 48000.0f;

        // Create a standalone XAudio2 engine (not tied to Skyrim's audio system)
        HRESULT hr = XAudio2Create(&ctx->xaEngine, 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(hr)) {
            logger::error("[Audio] XAudio2Create failed: 0x{:X}", static_cast<uint32_t>(hr));
            delete ctx;
            return nullptr;
        }

        // Create a mastering voice routing to the default output device
        hr = ctx->xaEngine->CreateMasteringVoice(&ctx->masterVoice);
        if (FAILED(hr)) {
            logger::error("[Audio] CreateMasteringVoice failed: 0x{:X}", static_cast<uint32_t>(hr));
            ctx->xaEngine->Release();
            delete ctx;
            return nullptr;
        }

        // Create stereo float32 source voice
        WAVEFORMATEX wfx{};
        wfx.wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
        wfx.nChannels       = 2;
        wfx.nSamplesPerSec  = static_cast<DWORD>(ctx->sampleRate);
        wfx.wBitsPerSample  = 32;
        wfx.nBlockAlign     = wfx.nChannels * (wfx.wBitsPerSample / 8);
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize          = 0;

        ctx->xaOutput.ctx = ctx;

        static constexpr float kDefaultVolume = 1.0f;

        hr = ctx->xaEngine->CreateSourceVoice(
            &ctx->xaOutput.sourceVoice,
            &wfx,
            0,                          // flags
            XAUDIO2_DEFAULT_FREQ_RATIO, // maxFrequencyRatio
            &ctx->xaOutput,             // voice callback
            nullptr,                    // send list (nullptr = mastering voice)
            nullptr                     // effect chain
        );

        if (FAILED(hr)) {
            logger::error("[Audio] Failed to create XAudio2 source voice: 0x{:X}",
                          static_cast<uint32_t>(hr));
            ctx->masterVoice->DestroyVoice();
            ctx->xaEngine->Release();
            delete ctx;
            return nullptr;
        }

        ctx->xaOutput.sourceVoice->SetVolume(kDefaultVolume);

        // Create destination node
        auto dest = std::make_unique<AudioDestinationNode>();
        dest->context = ctx;
        dest->channelCount = 2;
        ctx->destinationNode = dest.get();
        ctx->nodes.push_back(std::move(dest));

        ctx->state.store(AudioContextState::Suspended, std::memory_order_relaxed);

        logger::info("[Audio] AudioContext created (standalone XAudio2, sampleRate={})",
                     ctx->sampleRate);
        return ctx;
    }

    void ResumeAudioContext(AudioContext* ctx) {
        if (!ctx || ctx->destroyed.load(std::memory_order_acquire)) return;
        if (ctx->state.load(std::memory_order_acquire) == AudioContextState::Suspended) {
            ctx->state.store(AudioContextState::Running, std::memory_order_release);

            // Prime both buffers so XAudio2 has data immediately
            RenderAndSubmit(ctx);
            RenderAndSubmit(ctx);

            // Start the voice (begins consuming submitted buffers)
            // [013] Check HRESULT from Start()
            HRESULT hr = ctx->xaOutput.sourceVoice->Start();
            if (FAILED(hr)) {
                logger::error("[Audio] IXAudio2SourceVoice::Start() failed: {:08X}",
                              static_cast<uint32_t>(hr));
                ctx->state.store(AudioContextState::Suspended, std::memory_order_release);
            }

            logger::debug("[Audio] AudioContext resumed (XAudio2)");
        }
    }

    void SuspendAudioContext(AudioContext* ctx) {
        if (!ctx || ctx->destroyed.load(std::memory_order_acquire)) return;
        if (ctx->state.load(std::memory_order_acquire) == AudioContextState::Running) {
            ctx->xaOutput.sourceVoice->Stop();
            ctx->xaOutput.sourceVoice->FlushSourceBuffers();
            ctx->state.store(AudioContextState::Suspended, std::memory_order_release);
            logger::debug("[Audio] AudioContext suspended (XAudio2)");
        }
    }

    void DestroyAudioContext(AudioContext* ctx) {
        if (!ctx || ctx->destroyed.load(std::memory_order_acquire)) return;
        ctx->destroyed.store(true, std::memory_order_release);

        // [016] Signal the audio callback to stop re-entering before touching the voice.
        // OnBufferEnd checks this flag first; DestroyVoice() then blocks until any
        // in-flight callback has returned, so delete ctx is safe after that.
        ctx->destroying.store(true, std::memory_order_release);
        ctx->state.store(AudioContextState::Closed, std::memory_order_release);

        if (ctx->xaOutput.sourceVoice) {
            ctx->xaOutput.sourceVoice->Stop();
            ctx->xaOutput.sourceVoice->FlushSourceBuffers();
            // DestroyVoice() blocks until all in-flight callbacks have returned
            ctx->xaOutput.sourceVoice->DestroyVoice();
            ctx->xaOutput.sourceVoice = nullptr;
        }

        if (ctx->masterVoice) {
            ctx->masterVoice->DestroyVoice();
            ctx->masterVoice = nullptr;
        }

        if (ctx->xaEngine) {
            ctx->xaEngine->Release();
            ctx->xaEngine = nullptr;
        }

        ctx->nodes.clear();
        ctx->destinationNode = nullptr;
        ctx->buffers.clear();

        logger::debug("[Audio] AudioContext destroyed");
        delete ctx;
    }

    void CollectDeadNodes(AudioContext* ctx) {
        if (!ctx) return;

        // Phase 1: collect buffer pointers from orphaned source nodes, then erase them.
        // Safe because ctx->nodes is only appended-to by the JS thread (us), and the
        // audio thread never indexes into ctx->nodes (it traverses via graph edges).
        std::vector<AudioBuffer*> candidateBuffers;
        size_t deadNodeCount = 0;

        auto& nodes = ctx->nodes;
        for (auto it = nodes.begin(); it != nodes.end(); ) {
            AudioNode* n = it->get();
            if (n->type != AudioNode::Type::Destination &&
                n->graphOrphaned.load(std::memory_order_acquire)) {
                if (n->type == AudioNode::Type::BufferSource) {
                    auto* src = static_cast<AudioBufferSourceNode*>(n);
                    AudioBuffer* b = src->buffer.load(std::memory_order_relaxed);
                    if (b) {
                        candidateBuffers.push_back(b);
                        src->buffer.store(nullptr, std::memory_order_relaxed);
                    }
                }
                it = nodes.erase(it);
                ++deadNodeCount;
            } else {
                ++it;
            }
        }

        if (candidateBuffers.empty()) {
            ctx->orphanedNodeCount.store(0, std::memory_order_relaxed);
            return;
        }

        // Phase 2: remove buffers not referenced by any surviving node.
        auto& bufs = ctx->buffers;
        for (auto* deadBuf : candidateBuffers) {
            bool stillReferenced = false;
            for (auto& n : nodes) {
                if (n->type == AudioNode::Type::BufferSource) {
                    auto* s = static_cast<AudioBufferSourceNode*>(n.get());
                    if (s->buffer.load(std::memory_order_relaxed) == deadBuf) {
                        stillReferenced = true;
                        break;
                    }
                }
            }
            if (!stillReferenced) {
                for (auto bit = bufs.begin(); bit != bufs.end(); ++bit) {
                    if (bit->get() == deadBuf) {
                        bufs.erase(bit);
                        break;
                    }
                }
            }
        }

        ctx->orphanedNodeCount.store(0, std::memory_order_relaxed);
        // [017] First arg is dead-node count, second is candidate-buffer count
        logger::debug("[Audio] Collected {} dead nodes, {} candidate buffers",
                     deadNodeCount, candidateBuffers.size());
    }

}  // namespace PrismaUI::Audio
