#include <JavaScriptCore/JavaScript.h>

#include <cstring>

#include "AudioBridge.h"
#include "AudioBuffer.h"
#include "AudioCommandQueue.h"
#include "AudioContext.h"
#include "AudioGraph.h"
#include "AudioNodes.h"
#include "AudioParam.h"


namespace logger = SKSE::log;

namespace PrismaUI::Audio {

    // ---- Helpers ----

    static AudioNode* GetNode(JSObjectRef thisObject) {
        return static_cast<AudioNode*>(JSObjectGetPrivate(thisObject));
    }

    static AudioBuffer* GetBuffer(JSObjectRef thisObject) {
        return static_cast<AudioBuffer*>(JSObjectGetPrivate(thisObject));
    }

    // ---- Common node functions: connect / disconnect ----

    static JSValueRef Audio_nodeConnect(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                        const JSValueRef argv[], JSValueRef*) {
        auto* source = GetNode(thisObj);
        if (!source || argc < 1) return JSValueMakeUndefined(ctx);

        JSObjectRef destObj = JSValueToObject(ctx, argv[0], nullptr);
        if (!destObj) return JSValueMakeUndefined(ctx);

        auto* dest = static_cast<AudioNode*>(JSObjectGetPrivate(destObj));
        if (!dest || !source->context) return JSValueMakeUndefined(ctx);

        source->context->commandQueue_.TryPush({AudioCommand::Type::Connect, source, dest});

        return argv[0];  // Return the destination (per spec)
    }

    static JSValueRef Audio_nodeDisconnect(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                           const JSValueRef argv[], JSValueRef*) {
        auto* node = GetNode(thisObj);
        if (!node || !node->context) return JSValueMakeUndefined(ctx);

        if (argc > 0 && JSValueIsObject(ctx, argv[0])) {
            JSObjectRef destObj = JSValueToObject(ctx, argv[0], nullptr);
            auto* dest = destObj ? static_cast<AudioNode*>(JSObjectGetPrivate(destObj)) : nullptr;
            if (dest) {
                node->context->commandQueue_.TryPush({AudioCommand::Type::Disconnect, node, dest});
            }
        } else {
            node->context->commandQueue_.TryPush({AudioCommand::Type::DisconnectAll, node, nullptr});
        }

        return JSValueMakeUndefined(ctx);
    }

    // ---- GainNode ----

    static JSValueRef Audio_getGainParam(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* node = static_cast<GainNode*>(JSObjectGetPrivate(obj));
        if (!node) return JSValueMakeNull(ctx);
        return JSObjectMake(ctx, GetAudioParamClass(), &node->gain);
    }

    static JSStaticFunction kGainNodeFunctions[] = {
        {"connect", Audio_nodeConnect, kJSPropertyAttributeDontDelete},
        {"disconnect", Audio_nodeDisconnect, kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, 0}};

    static JSStaticValue kGainNodeValues[] = {
        {"gain", Audio_getGainParam, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, nullptr, 0}};

    static void AudioNodeFinalize(JSObjectRef obj) { JSObjectSetPrivate(obj, nullptr); }

    JSClassRef GetGainNodeClass() {
        static JSClassRef cls = []() {
            JSClassDefinition classDef{};
            classDef.className = "GainNode";
            classDef.staticFunctions = kGainNodeFunctions;
            classDef.staticValues = kGainNodeValues;
            classDef.finalize = AudioNodeFinalize;
            return JSClassCreate(&classDef);
        }();
        return cls;
    }

    // ---- AudioDestinationNode ----

    static JSStaticFunction kDestinationNodeFunctions[] = {
        {"connect", Audio_nodeConnect, kJSPropertyAttributeDontDelete},
        {"disconnect", Audio_nodeDisconnect, kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, 0}};

    JSClassRef GetAudioDestinationNodeClass() {
        static JSClassRef cls = []() {
            JSClassDefinition classDef{};
            classDef.className = "AudioDestinationNode";
            classDef.staticFunctions = kDestinationNodeFunctions;
            classDef.finalize = AudioNodeFinalize;
            return JSClassCreate(&classDef);
        }();
        return cls;
    }

    // ---- AudioBufferSourceNode ----

    static JSValueRef Audio_bufferSourceStart(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                              const JSValueRef argv[], JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(thisObj));
        if (!node) return JSValueMakeUndefined(ctx);

        double when = 0.0, offset = 0.0, duration = -1.0;
        if (argc > 0) when = JSValueToNumber(ctx, argv[0], nullptr);
        if (argc > 1) offset = JSValueToNumber(ctx, argv[1], nullptr);
        if (argc > 2) duration = JSValueToNumber(ctx, argv[2], nullptr);

        node->Start(when, offset, duration);

        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef Audio_bufferSourceStop(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                             const JSValueRef argv[], JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(thisObj));
        if (!node) return JSValueMakeUndefined(ctx);

        double when = 0.0;
        if (argc > 0) when = JSValueToNumber(ctx, argv[0], nullptr);

        node->Stop(when);

        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef Audio_getBuffer(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return JSValueMakeNull(ctx);
        AudioBuffer* buf = node->buffer.load(std::memory_order_relaxed);
        if (!buf) return JSValueMakeNull(ctx);
        return JSObjectMake(ctx, GetAudioBufferClass(), buf);
    }

    static bool Audio_setBuffer(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef val, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return false;

        if (JSValueIsNull(ctx, val) || JSValueIsUndefined(ctx, val)) {
            node->buffer.store(nullptr, std::memory_order_relaxed);
            return true;
        }

        JSObjectRef bufObj = JSValueToObject(ctx, val, nullptr);
        if (!bufObj) return false;

        auto* buf = static_cast<AudioBuffer*>(JSObjectGetPrivate(bufObj));
        node->buffer.store(buf, std::memory_order_relaxed);
        return true;
    }

    static JSValueRef Audio_getLoop(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, node->loop.load(std::memory_order_relaxed));
    }

    static bool Audio_setLoop(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef val, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return false;
        node->loop.store(JSValueToBoolean(ctx, val), std::memory_order_relaxed);
        return true;
    }

    static JSValueRef Audio_getLoopStart(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, node->loopStart.load(std::memory_order_relaxed));
    }

    static bool Audio_setLoopStart(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef val, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return false;
        node->loopStart.store(JSValueToNumber(ctx, val, nullptr), std::memory_order_relaxed);
        return true;
    }

    static JSValueRef Audio_getLoopEnd(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, node->loopEnd.load(std::memory_order_relaxed));
    }

    static bool Audio_setLoopEnd(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef val, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return false;
        node->loopEnd.store(JSValueToNumber(ctx, val, nullptr), std::memory_order_relaxed);
        return true;
    }

    static JSValueRef Audio_getPlaybackRate(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return JSValueMakeNull(ctx);
        return JSObjectMake(ctx, GetAudioParamClass(), &node->playbackRate);
    }

    static JSValueRef Audio_getEnded(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* node = static_cast<AudioBufferSourceNode*>(JSObjectGetPrivate(obj));
        if (!node) return JSValueMakeBoolean(ctx, false);
        return JSValueMakeBoolean(ctx, node->ended.load(std::memory_order_acquire));
    }

    static JSStaticFunction kBufferSourceFunctions[] = {
        {"connect", Audio_nodeConnect, kJSPropertyAttributeDontDelete},
        {"disconnect", Audio_nodeDisconnect, kJSPropertyAttributeDontDelete},
        {"start", Audio_bufferSourceStart, kJSPropertyAttributeDontDelete},
        {"stop", Audio_bufferSourceStop, kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, 0}};

    static JSStaticValue kBufferSourceValues[] = {
        {"buffer", Audio_getBuffer, Audio_setBuffer, kJSPropertyAttributeDontDelete},
        {"loop", Audio_getLoop, Audio_setLoop, kJSPropertyAttributeDontDelete},
        {"loopStart", Audio_getLoopStart, Audio_setLoopStart, kJSPropertyAttributeDontDelete},
        {"loopEnd", Audio_getLoopEnd, Audio_setLoopEnd, kJSPropertyAttributeDontDelete},
        {"playbackRate", Audio_getPlaybackRate, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"ended", Audio_getEnded, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, nullptr, 0}};

    JSClassRef GetBufferSourceNodeClass() {
        static JSClassRef cls = []() {
            JSClassDefinition classDef{};
            classDef.className = "AudioBufferSourceNode";
            classDef.staticFunctions = kBufferSourceFunctions;
            classDef.staticValues = kBufferSourceValues;
            classDef.finalize = AudioNodeFinalize;
            return JSClassCreate(&classDef);
        }();
        return cls;
    }

    // ---- AudioBuffer ----

    static JSValueRef Audio_getChannelData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                           const JSValueRef argv[], JSValueRef*) {
        auto* buf = GetBuffer(thisObj);
        if (!buf || argc < 1) return JSValueMakeNull(ctx);

        uint32_t ch = static_cast<uint32_t>(JSValueToNumber(ctx, argv[0], nullptr));
        if (ch >= buf->numberOfChannels) return JSValueMakeNull(ctx);

        size_t numFloats = buf->channelData[ch].size();
        size_t byteLen = numFloats * sizeof(float);

        float* copy = new float[numFloats];
        std::memcpy(copy, buf->channelData[ch].data(), byteLen);

        JSObjectRef arrayBuffer = JSObjectMakeArrayBufferWithBytesNoCopy(
            ctx, copy, byteLen, [](void* bytes, void*) { delete[] static_cast<float*>(bytes); }, nullptr, nullptr);

        if (!arrayBuffer) {
            delete[] copy;
            return JSValueMakeNull(ctx);
        }

        JSObjectRef float32Array = JSObjectMakeTypedArrayWithArrayBufferAndOffset(ctx, kJSTypedArrayTypeFloat32Array,
                                                                                  arrayBuffer, 0, numFloats, nullptr);

        return float32Array ? float32Array : JSValueMakeNull(ctx);
    }

    static JSValueRef Audio_copyFromChannel(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                            const JSValueRef argv[], JSValueRef*) {
        auto* buf = GetBuffer(thisObj);
        if (!buf || argc < 2) return JSValueMakeUndefined(ctx);

        JSObjectRef destArray = JSValueToObject(ctx, argv[0], nullptr);
        uint32_t ch = static_cast<uint32_t>(JSValueToNumber(ctx, argv[1], nullptr));
        uint32_t offset = (argc > 2) ? static_cast<uint32_t>(JSValueToNumber(ctx, argv[2], nullptr)) : 0;

        if (!destArray || ch >= buf->numberOfChannels) return JSValueMakeUndefined(ctx);

        JSTypedArrayType arrType = JSValueGetTypedArrayType(ctx, destArray, nullptr);
        if (arrType != kJSTypedArrayTypeFloat32Array) return JSValueMakeUndefined(ctx);

        size_t destLen = JSObjectGetTypedArrayLength(ctx, destArray, nullptr);
        void* destPtr = JSObjectGetTypedArrayBytesPtr(ctx, destArray, nullptr);
        if (!destPtr || destLen == 0) return JSValueMakeUndefined(ctx);

        const auto& channelData = buf->channelData[ch];
        size_t copyLen = destLen;
        if (offset + copyLen > channelData.size()) {
            copyLen = (channelData.size() > offset) ? channelData.size() - offset : 0;
        }

        if (copyLen > 0) {
            std::memcpy(destPtr, channelData.data() + offset, copyLen * sizeof(float));
        }

        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef Audio_copyToChannel(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                          const JSValueRef argv[], JSValueRef*) {
        auto* buf = GetBuffer(thisObj);
        if (!buf || argc < 2) return JSValueMakeUndefined(ctx);

        JSObjectRef srcArray = JSValueToObject(ctx, argv[0], nullptr);
        uint32_t ch = static_cast<uint32_t>(JSValueToNumber(ctx, argv[1], nullptr));
        uint32_t offset = (argc > 2) ? static_cast<uint32_t>(JSValueToNumber(ctx, argv[2], nullptr)) : 0;

        if (!srcArray || ch >= buf->numberOfChannels) return JSValueMakeUndefined(ctx);

        JSTypedArrayType arrType = JSValueGetTypedArrayType(ctx, srcArray, nullptr);
        if (arrType != kJSTypedArrayTypeFloat32Array) return JSValueMakeUndefined(ctx);

        size_t srcLen = JSObjectGetTypedArrayLength(ctx, srcArray, nullptr);
        void* srcPtr = JSObjectGetTypedArrayBytesPtr(ctx, srcArray, nullptr);
        if (!srcPtr || srcLen == 0) return JSValueMakeUndefined(ctx);

        auto& channelData = buf->channelData[ch];
        size_t copyLen = srcLen;
        if (offset + copyLen > channelData.size()) {
            copyLen = (channelData.size() > offset) ? channelData.size() - offset : 0;
        }

        if (copyLen > 0) {
            std::memcpy(channelData.data() + offset, srcPtr, copyLen * sizeof(float));
        }

        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef Audio_bufNumChannels(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* buf = GetBuffer(obj);
        return JSValueMakeNumber(ctx, buf ? buf->numberOfChannels : 0);
    }

    static JSValueRef Audio_bufLength(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* buf = GetBuffer(obj);
        return JSValueMakeNumber(ctx, buf ? buf->length : 0);
    }

    static JSValueRef Audio_bufSampleRate(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* buf = GetBuffer(obj);
        return JSValueMakeNumber(ctx, buf ? buf->sampleRate : 0);
    }

    static JSValueRef Audio_bufDuration(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* buf = GetBuffer(obj);
        return JSValueMakeNumber(ctx, buf ? buf->duration : 0);
    }

    static void AudioBufferFinalize(JSObjectRef obj) {
        // Buffer lifetime is owned by AudioContext::buffers, not by JS GC.
        JSObjectSetPrivate(obj, nullptr);
    }

    static JSStaticFunction kAudioBufferFunctions[] = {
        {"getChannelData", Audio_getChannelData, kJSPropertyAttributeDontDelete},
        {"copyFromChannel", Audio_copyFromChannel, kJSPropertyAttributeDontDelete},
        {"copyToChannel", Audio_copyToChannel, kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, 0}};

    static JSStaticValue kAudioBufferValues[] = {
        {"numberOfChannels", Audio_bufNumChannels, nullptr,
         kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"length", Audio_bufLength, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"sampleRate", Audio_bufSampleRate, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"duration", Audio_bufDuration, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, nullptr, 0}};

    JSClassRef GetAudioBufferClass() {
        static JSClassRef cls = []() {
            JSClassDefinition classDef{};
            classDef.className = "AudioBuffer";
            classDef.staticFunctions = kAudioBufferFunctions;
            classDef.staticValues = kAudioBufferValues;
            classDef.finalize = AudioBufferFinalize;
            return JSClassCreate(&classDef);
        }();
        return cls;
    }

    // ---- Factory functions (called from AudioBridge.cpp as AudioContext methods) ----

    static constexpr uint32_t kCollectThreshold = 32;

    JSValueRef Audio_createGain(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t, const JSValueRef[],
                                JSValueRef*) {
        auto* ac = static_cast<AudioContext*>(JSObjectGetPrivate(thisObj));
        if (!ac) return JSValueMakeNull(ctx);

        auto node = std::make_unique<GainNode>();
        node->context = ac;
        node->channelCount = 2;

        GainNode* raw = node.get();
        ac->nodes.push_back(std::move(node));

        return JSObjectMake(ctx, GetGainNodeClass(), raw);
    }

    JSValueRef Audio_createBufferSource(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t, const JSValueRef[],
                                        JSValueRef*) {
        auto* ac = static_cast<AudioContext*>(JSObjectGetPrivate(thisObj));
        if (!ac) return JSValueMakeNull(ctx);

        if (ac->orphanedNodeCount.load(std::memory_order_relaxed) >= kCollectThreshold) CollectDeadNodes(ac);

        auto node = std::make_unique<AudioBufferSourceNode>();
        node->context = ac;
        node->channelCount = 2;

        AudioBufferSourceNode* raw = node.get();
        ac->nodes.push_back(std::move(node));

        return JSObjectMake(ctx, GetBufferSourceNodeClass(), raw);
    }

    JSValueRef Audio_createBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                  const JSValueRef argv[], JSValueRef*) {
        auto* ac = static_cast<AudioContext*>(JSObjectGetPrivate(thisObj));
        if (!ac || argc < 3) return JSValueMakeNull(ctx);

        if (ac->orphanedNodeCount.load(std::memory_order_relaxed) >= kCollectThreshold) CollectDeadNodes(ac);

        uint32_t numChannels = static_cast<uint32_t>(JSValueToNumber(ctx, argv[0], nullptr));
        uint32_t length = static_cast<uint32_t>(JSValueToNumber(ctx, argv[1], nullptr));
        float sampleRate = static_cast<float>(JSValueToNumber(ctx, argv[2], nullptr));

        auto buf = CreateBuffer(numChannels, length, sampleRate);
        if (!buf) return JSValueMakeNull(ctx);

        AudioBuffer* raw = buf.get();
        ac->buffers.push_back(std::move(buf));

        return JSObjectMake(ctx, GetAudioBufferClass(), raw);
    }

    JSValueRef Audio_decodeAudioData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                     const JSValueRef argv[], JSValueRef* exception) {
        auto* ac = static_cast<AudioContext*>(JSObjectGetPrivate(thisObj));
        if (!ac || argc < 1) {
            if (exception) {
                JSStringRef msg = JSStringCreateWithUTF8CString("decodeAudioData requires an ArrayBuffer argument");
                *exception = JSValueMakeString(ctx, msg);
                JSStringRelease(msg);
            }
            return JSValueMakeNull(ctx);
        }

        JSObjectRef abObj = JSValueToObject(ctx, argv[0], nullptr);
        if (!abObj) {
            if (exception) {
                JSStringRef msg = JSStringCreateWithUTF8CString("Invalid ArrayBuffer");
                *exception = JSValueMakeString(ctx, msg);
                JSStringRelease(msg);
            }
            return JSValueMakeNull(ctx);
        }

        void* bytes = JSObjectGetArrayBufferBytesPtr(ctx, abObj, nullptr);
        size_t byteLen = JSObjectGetArrayBufferByteLength(ctx, abObj, nullptr);

        if (!bytes || byteLen == 0) {
            if (exception) {
                JSStringRef msg = JSStringCreateWithUTF8CString("Empty ArrayBuffer");
                *exception = JSValueMakeString(ctx, msg);
                JSStringRelease(msg);
            }
            return JSValueMakeNull(ctx);
        }

        auto buf = DecodeFromMemory(static_cast<const uint8_t*>(bytes), byteLen, ac->sampleRate);

        if (!buf) {
            if (exception) {
                JSStringRef msg = JSStringCreateWithUTF8CString("Failed to decode audio data");
                *exception = JSValueMakeString(ctx, msg);
                JSStringRelease(msg);
            }
            return JSValueMakeNull(ctx);
        }

        AudioBuffer* raw = buf.get();
        ac->buffers.push_back(std::move(buf));

        return JSObjectMake(ctx, GetAudioBufferClass(), raw);
    }

}  // namespace PrismaUI::Audio
