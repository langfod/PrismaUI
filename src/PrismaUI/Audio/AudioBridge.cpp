#include "AudioBridge.h"

#include <JavaScriptCore/JavaScript.h>

#include <cstring>
#include <string>

#include "AudioBuffer.h"
#include "AudioContext.h"
#include "AudioGraph.h"
#include "AudioNodes.h"
#include "PrismaUI/Core.h"


namespace logger = SKSE::log;

namespace PrismaUI::Audio {

    // ---- Helpers ----

    static AudioContext* GetAudioCtx(JSObjectRef thisObject) {
        return static_cast<AudioContext*>(JSObjectGetPrivate(thisObject));
    }

    static std::string JSStringToStd(JSContextRef ctx, JSValueRef val) {
        if (JSValueIsNull(ctx, val) || JSValueIsUndefined(ctx, val)) return "";
        JSStringRef jsStr = JSValueToStringCopy(ctx, val, nullptr);
        if (!jsStr) return "";
        size_t maxLen = JSStringGetMaximumUTF8CStringSize(jsStr);
        std::string result(maxLen, '\0');
        JSStringGetUTF8CString(jsStr, result.data(), maxLen);
        JSStringRelease(jsStr);
        result.resize(std::strlen(result.c_str()));
        return result;
    }

    // ---- AudioContext staticValues ----

    static JSValueRef Audio_getSampleRate(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* ac = GetAudioCtx(obj);
        if (!ac) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, ac->sampleRate);
    }

    static JSValueRef Audio_getCurrentTime(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* ac = GetAudioCtx(obj);
        if (!ac) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, ac->currentTime_.load(std::memory_order_acquire));
    }

    static JSValueRef Audio_getState(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* ac = GetAudioCtx(obj);
        if (!ac) {
            JSStringRef s = JSStringCreateWithUTF8CString("closed");
            JSValueRef v = JSValueMakeString(ctx, s);
            JSStringRelease(s);
            return v;
        }
        const char* stateStr = "suspended";
        switch (ac->state.load(std::memory_order_acquire)) {
            case AudioContextState::Running:
                stateStr = "running";
                break;
            case AudioContextState::Suspended:
                stateStr = "suspended";
                break;
            case AudioContextState::Closed:
                stateStr = "closed";
                break;
        }
        JSStringRef s = JSStringCreateWithUTF8CString(stateStr);
        JSValueRef v = JSValueMakeString(ctx, s);
        JSStringRelease(s);
        return v;
    }

    static JSValueRef Audio_getDestination(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* ac = GetAudioCtx(obj);
        if (!ac || !ac->destinationNode) return JSValueMakeNull(ctx);

        if (ac->cachedDestinationObj) {
            return static_cast<JSObjectRef>(ac->cachedDestinationObj);
        }

        JSObjectRef destObj = JSObjectMake(ctx, GetAudioDestinationNodeClass(), ac->destinationNode);
        JSValueProtect(ctx, destObj);
        ac->cachedDestinationObj = destObj;
        ac->cachedDestinationCtx = const_cast<void*>(static_cast<const void*>(ctx));
        return destObj;
    }

    static JSValueRef Audio_getBaseLatency(JSContextRef ctx, JSObjectRef, JSStringRef, JSValueRef*) {
        return JSValueMakeNumber(ctx, 0.01);
    }

    static JSValueRef MakeResolvedPromise(JSContextRef ctx) {
        JSObjectRef resolve = nullptr;
        JSObjectRef promise = JSObjectMakeDeferredPromise(ctx, &resolve, nullptr, nullptr);
        if (promise && resolve) {
            JSObjectCallAsFunction(ctx, resolve, nullptr, 0, nullptr, nullptr);
        }
        return promise ? static_cast<JSValueRef>(promise) : JSValueMakeUndefined(ctx);
    }

    // ---- AudioContext staticFunctions ----

    static JSValueRef Audio_resume(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t, const JSValueRef[],
                                   JSValueRef*) {
        auto* ac = GetAudioCtx(thisObj);
        if (ac) ResumeAudioContext(ac);
        return MakeResolvedPromise(ctx);
    }

    static JSValueRef Audio_suspend(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t, const JSValueRef[],
                                    JSValueRef*) {
        auto* ac = GetAudioCtx(thisObj);
        if (ac) SuspendAudioContext(ac);
        return MakeResolvedPromise(ctx);
    }

    static JSValueRef Audio_close(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t, const JSValueRef[],
                                  JSValueRef*) {
        auto* ac = GetAudioCtx(thisObj);
        if (ac) {
            SuspendAudioContext(ac);  // Stop voice if Running, no-op otherwise
            ac->state.store(AudioContextState::Closed, std::memory_order_release);
        }
        return MakeResolvedPromise(ctx);
    }

    // Forward declarations for node creation (implemented in AudioBridgeNodes.cpp)
    JSValueRef Audio_createGain(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                const JSValueRef argv[], JSValueRef* exc);
    JSValueRef Audio_createBufferSource(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                        const JSValueRef argv[], JSValueRef* exc);
    JSValueRef Audio_createBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                  const JSValueRef argv[], JSValueRef* exc);
    JSValueRef Audio_decodeAudioData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                     const JSValueRef argv[], JSValueRef* exc);

    // ---- JSClassRef definitions ----

    static JSStaticFunction kAudioContextFunctions[] = {
        {"resume", Audio_resume, kJSPropertyAttributeDontDelete},
        {"suspend", Audio_suspend, kJSPropertyAttributeDontDelete},
        {"close", Audio_close, kJSPropertyAttributeDontDelete},
        {"createGain", Audio_createGain, kJSPropertyAttributeDontDelete},
        {"createBufferSource", Audio_createBufferSource, kJSPropertyAttributeDontDelete},
        {"createBuffer", Audio_createBuffer, kJSPropertyAttributeDontDelete},
        {"decodeAudioData", Audio_decodeAudioData, kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, 0}};

    static JSStaticValue kAudioContextValues[] = {
        {"sampleRate", Audio_getSampleRate, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"currentTime", Audio_getCurrentTime, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"state", Audio_getState, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"destination", Audio_getDestination, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"baseLatency", Audio_getBaseLatency, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, nullptr, 0}};

    // AudioContext lifetime is managed by the View, not JS GC.
    static void AudioContextFinalize(JSObjectRef obj) { JSObjectSetPrivate(obj, nullptr); }

    JSClassRef GetAudioContextClass() {
        static JSClassRef cls = []() {
            JSClassDefinition classDef{};
            classDef.className = "AudioContext";
            classDef.staticFunctions = kAudioContextFunctions;
            classDef.staticValues = kAudioContextValues;
            classDef.finalize = AudioContextFinalize;
            return JSClassCreate(&classDef);
        }();
        return cls;
    }

    // ---- JS_CreateAudioContext (native function injected as global) ----

    static JSValueRef JS_CreateAudioContext(JSContextRef ctx, JSObjectRef function, [[maybe_unused]] JSObjectRef thisObject,
                                            size_t argc, const JSValueRef argv[], JSValueRef* exception) {
        float requestedSR = 0.0f;
        if (argc > 0 && JSValueIsNumber(ctx, argv[0])) {
            requestedSR = static_cast<float>(JSValueToNumber(ctx, argv[0], nullptr));
        }

        JSStringRef viewIdProp = JSStringCreateWithUTF8CString("__viewId");
        JSValueRef viewIdVal = JSObjectGetProperty(ctx, function, viewIdProp, nullptr);
        JSStringRelease(viewIdProp);

        if (!JSValueIsString(ctx, viewIdVal)) {
            logger::error("[Audio] __viewId is not a string — cannot link AudioContext");
            if (exception) {
                JSStringRef msg = JSStringCreateWithUTF8CString("AudioContext creation failed: missing viewId");
                *exception = JSValueMakeString(ctx, msg);
                JSStringRelease(msg);
            }
            return JSValueMakeNull(ctx);
        }

        std::string viewIdStr = JSStringToStd(ctx, viewIdVal);
        auto viewId = static_cast<uint64_t>(std::strtoull(viewIdStr.c_str(), nullptr, 10));

        std::unique_lock lock(Core::viewsMutex);
        auto it = Core::views.find(viewId);
        if (it == Core::views.end()) {
            logger::error("[Audio] View [{}] not found — refusing to create AudioContext", viewId);
            if (exception) {
                JSStringRef msg = JSStringCreateWithUTF8CString("AudioContext creation failed: view not found");
                *exception = JSValueMakeString(ctx, msg);
                JSStringRelease(msg);
            }
            return JSValueMakeNull(ctx);
        }

        AudioContext* audioCtx = CreateAudioContext(requestedSR);
        if (!audioCtx) {
            if (exception) {
                JSStringRef msg = JSStringCreateWithUTF8CString("Failed to create AudioContext");
                *exception = JSValueMakeString(ctx, msg);
                JSStringRelease(msg);
            }
            return JSValueMakeNull(ctx);
        }

        it->second->audioContext = audioCtx;
        lock.unlock();
        logger::info("[Audio] Linked AudioContext to View [{}]", viewId);

        JSObjectRef contextObj = JSObjectMake(ctx, GetAudioContextClass(), audioCtx);
        return contextObj;
    }

    // ---- InjectAudioBindings ----

    void InjectAudioBindings(JSContextRef jsCtx, uint64_t viewId) {
        JSObjectRef globalObj = JSContextGetGlobalObject(jsCtx);

        // Create __prismaCreateAudioContext function
        JSStringRef funcName = JSStringCreateWithUTF8CString("__prismaCreateAudioContext");
        JSObjectRef funcObj = JSObjectMakeFunctionWithCallback(jsCtx, funcName, JS_CreateAudioContext);

        // Store viewId as STRING on the function (uint64 exceeds JS 2^53 safe int limit)
        std::string viewIdStr = std::to_string(viewId);
        JSStringRef viewIdPropStr = JSStringCreateWithUTF8CString("__viewId");
        JSStringRef viewIdJSStr = JSStringCreateWithUTF8CString(viewIdStr.c_str());
        JSObjectSetProperty(jsCtx, funcObj, viewIdPropStr, JSValueMakeString(jsCtx, viewIdJSStr),
                            kJSPropertyAttributeDontEnum | kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(viewIdJSStr);
        JSStringRelease(viewIdPropStr);

        JSObjectSetProperty(jsCtx, globalObj, funcName, funcObj, kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(funcName);

        logger::info("[Audio] Audio bindings injected for view {}", viewId);
    }

}  // namespace PrismaUI::Audio
