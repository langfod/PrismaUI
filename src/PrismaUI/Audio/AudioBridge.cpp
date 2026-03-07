#include "AudioBridge.h"

#include "AudioBuffer.h"
#include "AudioContext.h"
#include "AudioGraph.h"
#include "AudioNodes.h"
#include "PrismaUI/Core.h"

#include <JavaScriptCore/JavaScript.h>

#include <cstring>
#include <string>

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

    static JSValueRef Audio_getSampleRate(JSContextRef ctx, JSObjectRef obj,
                                          JSStringRef, JSValueRef*) {
        auto* ac = GetAudioCtx(obj);
        if (!ac) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, ac->sampleRate);
    }

    static JSValueRef Audio_getCurrentTime(JSContextRef ctx, JSObjectRef obj,
                                            JSStringRef, JSValueRef*) {
        auto* ac = GetAudioCtx(obj);
        if (!ac) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, ac->currentTime_.load(std::memory_order_acquire));
    }

    static JSValueRef Audio_getState(JSContextRef ctx, JSObjectRef obj,
                                      JSStringRef, JSValueRef*) {
        auto* ac = GetAudioCtx(obj);
        if (!ac) {
            JSStringRef s = JSStringCreateWithUTF8CString("closed");
            JSValueRef v = JSValueMakeString(ctx, s);
            JSStringRelease(s);
            return v;
        }
        const char* stateStr = "suspended";
        switch (ac->state) {
            case AudioContextState::Running:   stateStr = "running"; break;
            case AudioContextState::Suspended: stateStr = "suspended"; break;
            case AudioContextState::Closed:    stateStr = "closed"; break;
        }
        JSStringRef s = JSStringCreateWithUTF8CString(stateStr);
        JSValueRef v = JSValueMakeString(ctx, s);
        JSStringRelease(s);
        return v;
    }

    static JSValueRef Audio_getDestination(JSContextRef ctx, JSObjectRef obj,
                                            JSStringRef, JSValueRef*) {
        auto* ac = GetAudioCtx(obj);
        if (!ac || !ac->destinationNode) return JSValueMakeNull(ctx);

        // Create a JSObject wrapping the destination node
        JSObjectRef destObj = JSObjectMake(ctx, GetAudioDestinationNodeClass(), ac->destinationNode);
        return destObj;
    }

    static JSValueRef Audio_getBaseLatency(JSContextRef ctx, JSObjectRef,
                                            JSStringRef, JSValueRef*) {
        return JSValueMakeNumber(ctx, 0.01);
    }

    // ---- AudioContext staticFunctions ----

    static JSValueRef Audio_resume(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                    size_t, const JSValueRef[], JSValueRef*) {
        auto* ac = GetAudioCtx(thisObj);
        if (ac) ResumeAudioContext(ac);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef Audio_suspend(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                     size_t, const JSValueRef[], JSValueRef*) {
        auto* ac = GetAudioCtx(thisObj);
        if (ac) SuspendAudioContext(ac);
        return JSValueMakeUndefined(ctx);
    }

    static JSValueRef Audio_close(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                   size_t, const JSValueRef[], JSValueRef*) {
        auto* ac = GetAudioCtx(thisObj);
        if (ac) {
            ac->state = AudioContextState::Closed;
            SuspendAudioContext(ac);
        }
        return JSValueMakeUndefined(ctx);
    }

    // Forward declarations for node creation (implemented in AudioBridgeNodes.cpp)
    JSValueRef Audio_createGain(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                size_t argc, const JSValueRef argv[], JSValueRef* exc);
    JSValueRef Audio_createBufferSource(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                         size_t argc, const JSValueRef argv[], JSValueRef* exc);
    JSValueRef Audio_createBuffer(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                   size_t argc, const JSValueRef argv[], JSValueRef* exc);
    JSValueRef Audio_decodeAudioData(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                      size_t argc, const JSValueRef argv[], JSValueRef* exc);

    // ---- JSClassRef definitions ----

    static JSStaticFunction kAudioContextFunctions[] = {
        {"resume",             Audio_resume,             kJSPropertyAttributeDontDelete},
        {"suspend",            Audio_suspend,            kJSPropertyAttributeDontDelete},
        {"close",              Audio_close,              kJSPropertyAttributeDontDelete},
        {"createGain",         Audio_createGain,         kJSPropertyAttributeDontDelete},
        {"createBufferSource", Audio_createBufferSource, kJSPropertyAttributeDontDelete},
        {"createBuffer",       Audio_createBuffer,       kJSPropertyAttributeDontDelete},
        {"decodeAudioData",    Audio_decodeAudioData,    kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, 0}
    };

    static JSStaticValue kAudioContextValues[] = {
        {"sampleRate",  Audio_getSampleRate,  nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"currentTime", Audio_getCurrentTime, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"state",       Audio_getState,       nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"destination", Audio_getDestination, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"baseLatency", Audio_getBaseLatency, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, nullptr, 0}
    };

    static JSClassRef g_AudioContextClass = nullptr;

    JSClassRef GetAudioContextClass() {
        if (!g_AudioContextClass) {
            JSClassDefinition classDef{};
            classDef.className = "AudioContext";
            classDef.staticFunctions = kAudioContextFunctions;
            classDef.staticValues = kAudioContextValues;
            g_AudioContextClass = JSClassCreate(&classDef);
        }
        return g_AudioContextClass;
    }

    // ---- JS_CreateAudioContext (native function injected as global) ----

    static JSValueRef JS_CreateAudioContext(JSContextRef ctx, JSObjectRef function,
                                             JSObjectRef /*thisObject*/,
                                             size_t argc, const JSValueRef argv[],
                                             JSValueRef* exception) {
        float requestedSR = 0.0f;
        if (argc > 0 && JSValueIsNumber(ctx, argv[0])) {
            requestedSR = static_cast<float>(JSValueToNumber(ctx, argv[0], nullptr));
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

        // Retrieve viewId from the function object (stored as string property)
        JSStringRef viewIdProp = JSStringCreateWithUTF8CString("__viewId");
        JSValueRef viewIdVal = JSObjectGetProperty(ctx, function, viewIdProp, nullptr);
        JSStringRelease(viewIdProp);

        if (JSValueIsString(ctx, viewIdVal)) {
            std::string viewIdStr = JSStringToStd(ctx, viewIdVal);
            auto viewId = static_cast<uint64_t>(std::strtoull(viewIdStr.c_str(), nullptr, 10));

            std::shared_lock lock(Core::viewsMutex);
            auto it = Core::views.find(viewId);
            if (it != Core::views.end()) {
                it->second->audioContext = audioCtx;
                logger::info("[Audio] Linked AudioContext to View [{}]", viewId);
            }
        }

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
        JSObjectSetProperty(jsCtx, funcObj, viewIdPropStr,
                            JSValueMakeString(jsCtx, viewIdJSStr),
                            kJSPropertyAttributeDontEnum | kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(viewIdJSStr);
        JSStringRelease(viewIdPropStr);

        // Set on global object
        JSObjectSetProperty(jsCtx, globalObj, funcName, funcObj,
                            kJSPropertyAttributeReadOnly, nullptr);
        JSStringRelease(funcName);

        logger::info("[Audio] Audio bindings injected for view {}", viewId);
    }

}  // namespace PrismaUI::Audio
