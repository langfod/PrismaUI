#include "Platform/GameInput.h"

#include <Windows.h>
#include <immintrin.h>

#include <algorithm>
#include <cstring>
#include <limits>

#include "Platform/Runtime.h"


namespace PrismaUI::Platform::GameInput {
    namespace {
        struct RawArray {
            Sink** data;
            std::uint32_t capacity;
            std::uint32_t pad0C;
            std::uint32_t size;
            std::uint32_t pad14;
        };
        static_assert(sizeof(RawArray) == 0x18);

        struct SpinLock {
            volatile LONG owningThread;
            volatile LONG lockCount;
        };
        static_assert(sizeof(SpinLock) == 0x8);

        struct EventSource {
            RawArray sinks;
            RawArray pendingRegisters;
            RawArray pendingUnregisters;
            SpinLock lock;
            bool notifying;
            std::byte padding[7];
        };
        static_assert(sizeof(EventSource) == 0x58);

        std::uintptr_t Resolved(AddressKey key) noexcept {
            return IsRuntimeInitialized() ? GetRelocations().Address(key).value_or(0) : 0;
        }

        void Lock(SpinLock& lock) noexcept {
            const auto threadId = static_cast<LONG>(GetCurrentThreadId());
            _mm_lfence();
            if (lock.owningThread == threadId) {
                InterlockedIncrement(&lock.lockCount);
                return;
            }
            std::uint32_t spinCount = 0;
            while (InterlockedCompareExchange(&lock.lockCount, 1, 0) != 0) {
                _mm_pause();
                if (++spinCount >= 10000) {
                    Sleep(1);
                }
            }
            _mm_lfence();
            lock.owningThread = threadId;
            _mm_sfence();
        }

        void Unlock(SpinLock& lock) noexcept {
            const auto threadId = static_cast<LONG>(GetCurrentThreadId());
            _mm_lfence();
            if (lock.owningThread != threadId) {
                return;
            }
            if (lock.lockCount == 1) {
                lock.owningThread = 0;
                _mm_mfence();
                InterlockedCompareExchange(&lock.lockCount, 0, 1);
            } else {
                InterlockedDecrement(&lock.lockCount);
            }
        }

        class LockGuard {
        public:
            explicit LockGuard(SpinLock& lock) noexcept : lock_(lock) { Lock(lock_); }
            ~LockGuard() { Unlock(lock_); }

        private:
            SpinLock& lock_;
        };

        void* Allocate(std::size_t size) noexcept {
            const auto getManagerAddress = Resolved(AddressKey::kMemoryManager);
            const auto allocateAddress = Resolved(AddressKey::kMemoryAllocate);
            if (!getManagerAddress || !allocateAddress) {
                return nullptr;
            }
            using GetManager = void* (*)();
            using AllocateMemory = void* (*)(void*, std::size_t, std::int32_t, bool);
            auto* manager = reinterpret_cast<GetManager>(getManagerAddress)();
            return manager ? reinterpret_cast<AllocateMemory>(allocateAddress)(manager, size, 0, false) : nullptr;
        }

        void* Reallocate(void* memory, std::size_t size) noexcept {
            const auto getManagerAddress = Resolved(AddressKey::kMemoryManager);
            const auto reallocateAddress = Resolved(AddressKey::kMemoryReallocate);
            if (!getManagerAddress || !reallocateAddress) {
                return nullptr;
            }
            using GetManager = void* (*)();
            using ReallocateMemory = void* (*)(void*, void*, std::size_t, std::int32_t, bool);
            auto* manager = reinterpret_cast<GetManager>(getManagerAddress)();
            return manager ? reinterpret_cast<ReallocateMemory>(reallocateAddress)(manager, memory, size, 0, false)
                           : nullptr;
        }

        bool Contains(const RawArray& array, const Sink* sink) noexcept {
            return array.data && std::find(array.data, array.data + array.size, sink) != array.data + array.size;
        }

        bool Append(RawArray& array, Sink* sink) noexcept {
            if (Contains(array, sink)) {
                return true;
            }
            if (array.size == array.capacity) {
                const auto newCapacity = array.capacity == 0 ? 4U : array.capacity * 2U;
                if (newCapacity < array.capacity ||
                    newCapacity > std::numeric_limits<std::uint32_t>::max() / sizeof(Sink*)) {
                    return false;
                }
                const auto bytes = static_cast<std::size_t>(newCapacity) * sizeof(Sink*);
                void* newData = array.data ? Reallocate(array.data, bytes) : Allocate(bytes);
                if (!newData) {
                    return false;
                }
                array.data = static_cast<Sink**>(newData);
                array.capacity = newCapacity;
            }
            array.data[array.size++] = sink;
            return true;
        }

        void Erase(RawArray& array, const Sink* sink) noexcept {
            if (!array.data) {
                return;
            }
            const auto it = std::find(array.data, array.data + array.size, sink);
            if (it == array.data + array.size) {
                return;
            }
            std::move(it + 1, array.data + array.size, it);
            --array.size;
        }

        EventSource* Source() noexcept {
            const auto address = Resolved(AddressKey::kInputDeviceManager);
            return address ? static_cast<EventSource*>(*reinterpret_cast<void**>(address)) : nullptr;
        }

        const std::byte* Bytes(const void* object) noexcept { return static_cast<const std::byte*>(object); }
    }

    std::uint32_t ButtonEvent::IdCode() const noexcept {
        return *reinterpret_cast<const std::uint32_t*>(Bytes(this) + 0x20);
    }

    Device ButtonEvent::InputDevice() const noexcept { return *reinterpret_cast<const Device*>(Bytes(this) + 0x08); }

    float ButtonEvent::Value() const noexcept {
        const auto offset = GetRuntimeContext().Family() == RuntimeFamily::kVR ? 0x30U : 0x28U;
        return *reinterpret_cast<const float*>(Bytes(this) + offset);
    }

    float ButtonEvent::HeldDuration() const noexcept {
        const auto offset = GetRuntimeContext().Family() == RuntimeFamily::kVR ? 0x34U : 0x2CU;
        return *reinterpret_cast<const float*>(Bytes(this) + offset);
    }

    bool ButtonEvent::IsDown() const noexcept { return Value() > 0.0F && HeldDuration() == 0.0F; }

    bool ButtonEvent::IsUp() const noexcept { return Value() == 0.0F && HeldDuration() > 0.0F; }

    bool ThumbstickEvent::IsLeft() const noexcept {
        return *reinterpret_cast<const std::uint32_t*>(Bytes(this) + 0x20) == 0x0B;
    }

    bool ThumbstickEvent::IsRight() const noexcept {
        return *reinterpret_cast<const std::uint32_t*>(Bytes(this) + 0x20) == 0x0C;
    }

    float ThumbstickEvent::X() const noexcept { return *reinterpret_cast<const float*>(Bytes(this) + 0x28); }

    float ThumbstickEvent::Y() const noexcept { return *reinterpret_cast<const float*>(Bytes(this) + 0x2C); }

    bool AddSink(Sink* sink) noexcept {
        auto* source = Source();
        if (!source || !sink) {
            return false;
        }
        LockGuard guard(source->lock);
        auto& destination = source->notifying ? source->pendingRegisters : source->sinks;
        if (!Append(destination, sink)) {
            return false;
        }
        Erase(source->pendingUnregisters, sink);
        return true;
    }

    void RemoveSink(Sink* sink) noexcept {
        auto* source = Source();
        if (!source || !sink) {
            return;
        }
        LockGuard guard(source->lock);
        if (source->notifying) {
            Append(source->pendingUnregisters, sink);
        } else {
            Erase(source->sinks, sink);
        }
        Erase(source->pendingRegisters, sink);
    }
}
