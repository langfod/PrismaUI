#pragma once

#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>

namespace PrismaUI::Audio {

    struct AudioParamEvent {
        enum class Type {
            SetValue,
            LinearRamp,
            ExponentialRamp,
            SetTarget,
            CancelScheduled
        };

        Type type;
        float value = 0.0f;
        double time = 0.0;
        // For SetTarget: timeConstant
        float timeConstant = 0.0f;
    };

    struct AudioParam {
        std::atomic<float> value{0.0f};
        float defaultValue = 0.0f;
        float minValue = -3.4028235e+38f;
        float maxValue = 3.4028235e+38f;

        // Scheduled events (guarded by AudioContext::graphMutex_)
        std::vector<AudioParamEvent> events;

        explicit AudioParam(float initial = 0.0f)
            : value(initial), defaultValue(initial) {}

        // Evaluate the param at a given time, processing scheduled events.
        // Called from the audio render thread (under graphMutex_).
        float Evaluate(double time);

        // Schedule methods (called from JS thread under graphMutex_)
        void ScheduleSetValueAtTime(float val, double time);
        void ScheduleLinearRampToValueAtTime(float val, double endTime);
        void ScheduleExponentialRampToValueAtTime(float val, double endTime);
        void ScheduleSetTargetAtTime(float target, double startTime, float timeConstant);
        void CancelScheduledValues(double startTime);
    };

}  // namespace PrismaUI::Audio
