#include "AudioParam.h"

#include <algorithm>

namespace PrismaUI::Audio {

    float AudioParam::Evaluate(double time) {
        if (events.empty()) {
            return value.load(std::memory_order_relaxed);
        }

        float currentValue = value.load(std::memory_order_relaxed);

        for (size_t i = 0; i < events.size(); ++i) {
            const auto& evt = events[i];

            switch (evt.type) {
                case AudioParamEvent::Type::SetValue: {
                    if (time >= evt.time) {
                        currentValue = evt.value;
                    }
                    break;
                }

                case AudioParamEvent::Type::LinearRamp: {
                    // Find the previous event's value and time as the ramp start
                    float startValue = defaultValue;
                    double startTime = 0.0;
                    if (i > 0) {
                        startValue = events[i - 1].value;
                        startTime = events[i - 1].time;
                    }

                    if (time >= evt.time) {
                        currentValue = evt.value;
                    } else if (time > startTime) {
                        double progress = (time - startTime) / (evt.time - startTime);
                        currentValue = startValue + static_cast<float>(progress) * (evt.value - startValue);
                    }
                    break;
                }

                case AudioParamEvent::Type::ExponentialRamp: {
                    float startValue = defaultValue;
                    double startTime = 0.0;
                    if (i > 0) {
                        startValue = events[i - 1].value;
                        startTime = events[i - 1].time;
                    }

                    // Exponential ramp requires non-zero, same-sign values
                    if (startValue == 0.0f) startValue = 1e-7f;
                    float targetVal = evt.value;
                    if (targetVal == 0.0f) targetVal = 1e-7f;

                    if (time >= evt.time) {
                        currentValue = evt.value;
                    } else if (time > startTime && evt.time > startTime) {
                        double progress = (time - startTime) / (evt.time - startTime);
                        float ratio = targetVal / startValue;
                        currentValue = startValue * std::pow(ratio, static_cast<float>(progress));
                    }
                    break;
                }

                case AudioParamEvent::Type::SetTarget: {
                    if (time >= evt.time) {
                        float startValue = defaultValue;
                        if (i > 0) {
                            startValue = events[i - 1].value;
                        }
                        double elapsed = time - evt.time;
                        float tc = evt.timeConstant > 0.0f ? evt.timeConstant : 0.001f;
                        currentValue = evt.value +
                            (startValue - evt.value) * std::exp(static_cast<float>(-elapsed / tc));
                    }
                    break;
                }

                case AudioParamEvent::Type::CancelScheduled:
                    // Already handled at scheduling time
                    break;
            }
        }

        return currentValue;
    }

    void AudioParam::ScheduleSetValueAtTime(float val, double time) {
        AudioParamEvent evt;
        evt.type = AudioParamEvent::Type::SetValue;
        evt.value = val;
        evt.time = time;
        events.push_back(evt);
        // Keep events sorted by time
        std::stable_sort(events.begin(), events.end(),
            [](const AudioParamEvent& a, const AudioParamEvent& b) { return a.time < b.time; });
    }

    void AudioParam::ScheduleLinearRampToValueAtTime(float val, double endTime) {
        AudioParamEvent evt;
        evt.type = AudioParamEvent::Type::LinearRamp;
        evt.value = val;
        evt.time = endTime;
        events.push_back(evt);
        std::stable_sort(events.begin(), events.end(),
            [](const AudioParamEvent& a, const AudioParamEvent& b) { return a.time < b.time; });
    }

    void AudioParam::ScheduleExponentialRampToValueAtTime(float val, double endTime) {
        AudioParamEvent evt;
        evt.type = AudioParamEvent::Type::ExponentialRamp;
        evt.value = val;
        evt.time = endTime;
        events.push_back(evt);
        std::stable_sort(events.begin(), events.end(),
            [](const AudioParamEvent& a, const AudioParamEvent& b) { return a.time < b.time; });
    }

    void AudioParam::ScheduleSetTargetAtTime(float target, double startTime, float timeConstant) {
        AudioParamEvent evt;
        evt.type = AudioParamEvent::Type::SetTarget;
        evt.value = target;
        evt.time = startTime;
        evt.timeConstant = timeConstant;
        events.push_back(evt);
        std::stable_sort(events.begin(), events.end(),
            [](const AudioParamEvent& a, const AudioParamEvent& b) { return a.time < b.time; });
    }

    void AudioParam::CancelScheduledValues(double startTime) {
        events.erase(
            std::remove_if(events.begin(), events.end(),
                [startTime](const AudioParamEvent& evt) { return evt.time >= startTime; }),
            events.end());
    }

}  // namespace PrismaUI::Audio
