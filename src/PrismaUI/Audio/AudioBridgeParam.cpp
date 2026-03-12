#include <JavaScriptCore/JavaScript.h>

#include "AudioBridge.h"
#include "AudioParam.h"

namespace PrismaUI::Audio {

    // ---- Helpers ----

    static AudioParam* GetParam(JSObjectRef thisObject) {
        return static_cast<AudioParam*>(JSObjectGetPrivate(thisObject));
    }

    // ---- AudioParam staticValues ----

    static JSValueRef AudioParam_getValue(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* p = GetParam(obj);
        if (!p) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, p->value.load(std::memory_order_relaxed));
    }

    static bool AudioParam_setValue(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef val, JSValueRef*) {
        auto* p = GetParam(obj);
        if (!p) return false;
        float v = static_cast<float>(JSValueToNumber(ctx, val, nullptr));
        p->value.store(v, std::memory_order_relaxed);
        return true;
    }

    static JSValueRef AudioParam_getDefaultValue(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* p = GetParam(obj);
        if (!p) return JSValueMakeNumber(ctx, 0);
        return JSValueMakeNumber(ctx, p->defaultValue);
    }

    static JSValueRef AudioParam_getMinValue(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* p = GetParam(obj);
        if (!p) return JSValueMakeNumber(ctx, -3.4028235e+38);
        return JSValueMakeNumber(ctx, p->minValue);
    }

    static JSValueRef AudioParam_getMaxValue(JSContextRef ctx, JSObjectRef obj, JSStringRef, JSValueRef*) {
        auto* p = GetParam(obj);
        if (!p) return JSValueMakeNumber(ctx, 3.4028235e+38);
        return JSValueMakeNumber(ctx, p->maxValue);
    }

    // ---- AudioParam staticFunctions ----

    static JSValueRef AudioParam_setValueAtTime(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                                const JSValueRef argv[], JSValueRef*) {
        auto* p = GetParam(thisObj);
        if (!p || argc < 2) return thisObj;
        float val = static_cast<float>(JSValueToNumber(ctx, argv[0], nullptr));
        double time = JSValueToNumber(ctx, argv[1], nullptr);
        p->ScheduleSetValueAtTime(val, time);
        return thisObj;
    }

    static JSValueRef AudioParam_linearRampToValueAtTime(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                                         size_t argc, const JSValueRef argv[], JSValueRef*) {
        auto* p = GetParam(thisObj);
        if (!p || argc < 2) return thisObj;
        float val = static_cast<float>(JSValueToNumber(ctx, argv[0], nullptr));
        double endTime = JSValueToNumber(ctx, argv[1], nullptr);
        p->ScheduleLinearRampToValueAtTime(val, endTime);
        return thisObj;
    }

    static JSValueRef AudioParam_exponentialRampToValueAtTime(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj,
                                                              size_t argc, const JSValueRef argv[],
                                                              JSValueRef* exception) {
        auto* p = GetParam(thisObj);
        if (!p || argc < 2) return thisObj;
        float val = static_cast<float>(JSValueToNumber(ctx, argv[0], nullptr));
        if (val <= 0.0f) {
            if (exception) {
                JSStringRef msg = JSStringCreateWithUTF8CString("exponentialRampToValueAtTime: value must be positive");
                *exception = JSValueMakeString(ctx, msg);
                JSStringRelease(msg);
            }
            return thisObj;
        }
        double endTime = JSValueToNumber(ctx, argv[1], nullptr);
        p->ScheduleExponentialRampToValueAtTime(val, endTime);
        return thisObj;
    }

    static JSValueRef AudioParam_setTargetAtTime(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                                 const JSValueRef argv[], JSValueRef*) {
        auto* p = GetParam(thisObj);
        if (!p || argc < 3) return thisObj;
        float target = static_cast<float>(JSValueToNumber(ctx, argv[0], nullptr));
        double startTime = JSValueToNumber(ctx, argv[1], nullptr);
        float timeConstant = static_cast<float>(JSValueToNumber(ctx, argv[2], nullptr));
        p->ScheduleSetTargetAtTime(target, startTime, timeConstant);
        return thisObj;
    }

    static JSValueRef AudioParam_setValueCurveAtTime([[maybe_unused]] JSContextRef ctx, [[maybe_unused]] JSObjectRef,
                                                     JSObjectRef thisObj, [[maybe_unused]] size_t,
                                                     [[maybe_unused]] const JSValueRef[],
                                                     [[maybe_unused]] JSValueRef*) {
        // TODO ? not yet implemented
        return thisObj;
    }

    static JSValueRef AudioParam_cancelScheduledValues(JSContextRef ctx, JSObjectRef, JSObjectRef thisObj, size_t argc,
                                                       const JSValueRef argv[], JSValueRef*) {
        auto* p = GetParam(thisObj);
        if (!p || argc < 1) return thisObj;
        double startTime = JSValueToNumber(ctx, argv[0], nullptr);
        p->CancelScheduledValues(startTime);
        return thisObj;
    }

    // ---- JSClassRef ----

    static JSStaticFunction kAudioParamFunctions[] = {
        {"setValueAtTime", AudioParam_setValueAtTime, kJSPropertyAttributeDontDelete},
        {"linearRampToValueAtTime", AudioParam_linearRampToValueAtTime, kJSPropertyAttributeDontDelete},
        {"exponentialRampToValueAtTime", AudioParam_exponentialRampToValueAtTime, kJSPropertyAttributeDontDelete},
        {"setTargetAtTime", AudioParam_setTargetAtTime, kJSPropertyAttributeDontDelete},
        {"setValueCurveAtTime", AudioParam_setValueCurveAtTime, kJSPropertyAttributeDontDelete},
        {"cancelScheduledValues", AudioParam_cancelScheduledValues, kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, 0}};

    static JSStaticValue kAudioParamValues[] = {
        {"value", AudioParam_getValue, AudioParam_setValue, kJSPropertyAttributeDontDelete},
        {"defaultValue", AudioParam_getDefaultValue, nullptr,
         kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"minValue", AudioParam_getMinValue, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {"maxValue", AudioParam_getMaxValue, nullptr, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete},
        {nullptr, nullptr, nullptr, 0}};

    static void AudioParamFinalize(JSObjectRef obj) { JSObjectSetPrivate(obj, nullptr); }

    JSClassRef GetAudioParamClass() {
        static JSClassRef cls = []() {
            JSClassDefinition classDef{};
            classDef.className = "AudioParam";
            classDef.staticFunctions = kAudioParamFunctions;
            classDef.staticValues = kAudioParamValues;
            classDef.finalize = AudioParamFinalize;
            return JSClassCreate(&classDef);
        }();
        return cls;
    }

}  // namespace PrismaUI::Audio
