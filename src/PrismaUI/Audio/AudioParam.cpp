#include "AudioParam.h"

#include <algorithm>
#include <cmath>

namespace PrismaUI::Audio {

    // Evaluates the parameter at `time` using events[0..limit-1] only.
    // Single-pass O(N) — maintains a running value at each event boundary.
    // Handles [030]: SetTarget uses the actual computed value at evt.time (not events[i-1].value).
    // Handles [032]: guards against division by zero when two events share the same time.
    // Handles [033]: guards ExponentialRamp against sign-crossing (which produces NaN via pow).
    // Caller must hold a lock on eventsMutex.
    static float EvaluateAt(const std::vector<AudioParamEvent>& events,
                             [[maybe_unused]] float defaultValue, float initial, double time,
                             size_t limit) {
        float currentValue = initial;
        // Tracks the accumulated value at each event's time boundary (for SetTarget startValue).
        float valueAtBoundary = initial;
        size_t count = std::min(limit, events.size());

        for (size_t i = 0; i < count; ++i) {
            const auto& evt = events[i];

            // Compute valueAtBoundary: what currentValue would be at evt.time
            // given all events [0..i-1]. We use the previous iteration's currentValue
            // but need to evaluate it at evt.time rather than at `time`.
            // For events that have already completed (time >= evt.time), currentValue
            // already reflects the settled value. For SetTarget, we need the precise
            // value at evt.time, which is valueAtBoundary from the previous event's
            // settled state.

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
                        // [032] Guard against division by zero when events share the same time
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

                    // [033] Clamp to avoid pow(negative, non-integer) → NaN on sign crossings
                    if ((startValue < 0.0f) != (targetVal < 0.0f)) {
                        startValue = std::copysign(1e-7f, targetVal);
                    }

                    if (time >= evt.time) {
                        valueAtBoundary = evt.value;
                        currentValue = evt.value;
                    } else if (time > startTime) {
                        // [032] Guard against division by zero
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
                        // [030] Use valueAtBoundary as the starting value — this is the
                        // accumulated value at evt.time from all preceding events, computed
                        // iteratively (O(1)) instead of recursively (O(n)).
                        float startValue = valueAtBoundary;
                        double elapsed = time - evt.time;
                        float tc = evt.timeConstant > 0.0f ? evt.timeConstant : 0.001f;
                        currentValue = evt.value +
                            (startValue - evt.value) * std::exp(static_cast<float>(-elapsed / tc));
                        // Update boundary: the settled value at the *next* event's time
                        // would be this exponential evaluated at that future time, but we
                        // approximate with currentValue (evaluated at `time`).  When the
                        // next event's time == `time` this is exact; otherwise, the next
                        // iteration will see the SetTarget's in-progress value.
                        valueAtBoundary = currentValue;
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
        events.erase(
            std::remove_if(events.begin(), events.end(),
                [startTime](const AudioParamEvent& evt) { return evt.time >= startTime; }),
            events.end());
        if (events.empty()) {
            hasEvents.store(false, std::memory_order_release);
        }
    }

}  // namespace PrismaUI::Audio
