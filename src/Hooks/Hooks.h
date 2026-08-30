#pragma once

#include <atomic>

namespace Hooks {
    struct D3DPresentHook
    {
        using D3DPresentFunc = void __fastcall(std::uint32_t);
        static void Install(D3DPresentFunc* func, std::atomic<D3DPresentFunc*>& original);
    };
}
