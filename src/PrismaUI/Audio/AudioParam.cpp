#include "AudioParam.h"

#include <algorithm>
#include <cmath>

namespace PrismaUI::Audio {

    static float EvaluateAt(const std::vector<AudioParamEvent>& events, [[maybe_unused]] float defaultValue,
                            float initial, double time, size_t limit) {
        float currentValue = initial;
        float valueAtBoundary = initial;
        size_t count = std::min(limit, events.size());

        for (size_t i = 0; i < count; ++i) {
            const auto& evt = events[i];

            switch (evt.type) {
                case AudioParamEvent::Type::SetValue: {
                    if (time >= evt.time) {
                        valueAtBoundary = evt.value;
                        currentValue = evt.value;
                    }
                    break;
                }

                case AudioParamEvent::Type::LinearRamp: {
                    float startValue = valueAtBoundary;
                    double startTime = (i > 0) ? events[i - 1].time : 0.0;

                    if (time >= evt.time) {
                        valueAtBoundary = evt.value;
                        currentValue = evt.value;
                    } else if (time > startTime) {
                        if (evt.time == startTime) {
                            currentValue = evt.value;
                        } else {
                            double progress = (time - startTime) / (evt.time - startTime);
                            currentValue = startValue + static_cast<float>(progress) * (evt.value - startValue);
                        }
                    }
                    break;
                }

                case AudioParamEvent::Type::ExponentialRamp: {
                    float startValue = valueAtBoundary;
                    double startTime = (i > 0) ? events[i - 1].time : 0.0;

                    if (startValue == 0.0f) startValue = 1e-7f;
                    float targetVal = (evt.value == 0.0f) ? 1e-7f : evt.value;

                    if ((startValue < 0.0f) != (targetVal < 0.0f)) {
                        startValue = std::copysign(1e-7f, targetVal);
                    }

                    if (time >= evt.time) {
                        valueAtBoundary = evt.value;
                        currentValue = evt.value;
                    } else if (time > startTime) {
                        if (evt.time == startTime) {
                            currentValue = evt.value;
                        } else {
                            double progress = (time - startTime) / (evt.time - startTime);
                            float ratio = targetVal / startValue;
                            currentValue = startValue * std::pow(ratio, static_cast<float>(progress));
                        }
                    }
                    break;
                }

                case AudioParamEvent::Type::SetTarget: {
                    if (time >= evt.time) {
                        float startValue = valueAtBoundary;
                        double elapsed = time - evt.time;
                        float tc = evt.timeConstant > 0.0f ? evt.timeConstant : 0.001f;
                        currentValue =
                            evt.value + (startValue - evt.value) * std::exp(static_cast<float>(-elapsed / tc));
                        valueAtBoundary = currentValue;
                    }
                    break;
                }

                case AudioParamEvent::Type::CancelScheduled:
                    break;
            }
        }

        return currentValue;
    }

    float AudioParam::Evaluate(double time) {
        if (!hasEvents.load(std::memory_order_relaxed)) {
            return value.load(std::memory_order_relaxed);
        }
        std::lock_guard<std::mutex> lock(eventsMutex);
        if (events.empty()) {
            return value.load(std::memory_order_relaxed);
        }
        float initial = value.load(std::memory_order_relaxed);
        return EvaluateAt(events, defaultValue, initial, time, events.size());
    }

    void AudioParam::ScheduleSetValueAtTime(float val, double time) {
        std::lock_guard<std::mutex> lock(eventsMutex);
        AudioParamEvent evt;
        evt.type = AudioParamEvent::Type::SetValue;
        evt.value = val;
        evt.time = time;
        events.push_back(evt);
        std::stable_sort(events.begin(), events.end(),
                         [](const AudioParamEvent& a, const AudioParamEvent& b) { return a.time < b.time; });
        hasEvents.store(true, std::memory_order_release);
    }

    void AudioParam::ScheduleLinearRampToValueAtTime(float val, double endTime) {
        std::lock_guard<std::mutex> lock(eventsMutex);
        AudioParamEvent evt;
        evt.type = AudioParamEvent::Type::LinearRamp;
        evt.value = val;
        evt.time = endTime;
        events.push_back(evt);
        std::stable_sort(events.begin(), events.end(),
                         [](const AudioParamEvent& a, const AudioParamEvent& b) { return a.time < b.time; });
        hasEvents.store(true, std::memory_order_release);
    }

    void AudioParam::ScheduleExponentialRampToValueAtTime(float val, double endTime) {
        std::lock_guard<std::mutex> lock(eventsMutex);
        AudioParamEvent evt;
        evt.type = AudioParamEvent::Type::ExponentialRamp;
        evt.value = val;
        evt.time = endTime;
        events.push_back(evt);
        std::stable_sort(events.begin(), events.end(),
                         [](const AudioParamEvent& a, const AudioParamEvent& b) { return a.time < b.time; });
        hasEvents.store(true, std::memory_order_release);
    }

    void AudioParam::ScheduleSetTargetAtTime(float target, double startTime, float timeConstant) {
        std::lock_guard<std::mutex> lock(eventsMutex);
        AudioParamEvent evt;
        evt.type = AudioParamEvent::Type::SetTarget;
        evt.value = target;
        evt.time = startTime;
        evt.timeConstant = timeConstant;
        events.push_back(evt);
        std::stable_sort(events.begin(), events.end(),
                         [](const AudioParamEvent& a, const AudioParamEvent& b) { return a.time < b.time; });
        hasEvents.store(true, std::memory_order_release);
    }

    void AudioParam::CancelScheduledValues(double startTime) {
        std::lock_guard<std::mutex> lock(eventsMutex);
        events.erase(std::remove_if(events.begin(), events.end(),
                                    [startTime](const AudioParamEvent& evt) { return evt.time >= startTime; }),
                     events.end());
        if (events.empty()) {
            hasEvents.store(false, std::memory_order_release);
        }
    }

}  // namespace PrismaUI::Audio
