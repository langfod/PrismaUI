#pragma once

#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>

namespace PrismaUI::Audio {

    struct AudioParamEvent {
        enum class Type { SetValue, LinearRamp, ExponentialRamp, SetTarget, CancelScheduled };

        Type type = Type::SetValue;
        float value = 0.0f;
        double time = 0.0;
        float timeConstant = 0.0f;
    };

    struct AudioParam {
        std::atomic<float> value{0.0f};
        float defaultValue = 0.0f;
        float minValue = -3.4028235e+38f;
        float maxValue = 3.4028235e+38f;

        std::mutex eventsMutex;
        std::vector<AudioParamEvent> events;

        std::atomic<bool> hasEvents{false};

        explicit AudioParam(float initial = 0.0f) : value(initial), defaultValue(initial) {}

        float Evaluate(double time);

        void ScheduleSetValueAtTime(float val, double time);
        void ScheduleLinearRampToValueAtTime(float val, double endTime);
        void ScheduleExponentialRampToValueAtTime(float val, double endTime);
        void ScheduleSetTargetAtTime(float target, double startTime, float timeConstant);
        void CancelScheduledValues(double startTime);
    };

}  // namespace PrismaUI::Audio
