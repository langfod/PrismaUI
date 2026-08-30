# PrismaUI Framework

SKSE plugin (DLL) that renders HTML/CSS/JS UI overlays in Skyrim using **Ultralight** (headless browser) + **DirectX 11** SpriteBatch.

> **For the AI agent:** After implementing any non-trivial feature or pattern, update this file — add or extend sections, note gotchas found during the work. Keep it concise and factual. Do not document what the code already says; only record things that would surprise a reader or aren't obvious from the source.

## Source layout

```text
src/
├── PrismaUI_API.h            — public modder header (copy into mod project)
├── Platform/                 — raw SKSE host, Address Library, relocation/patching, game ABI services
├── PrismaUI/
│   ├── Core.{h,cpp}          — global state, renderer init, D3DPresent hook entry
│   ├── ViewManager.{h,cpp}   — Create/Show/Hide/Focus/Destroy views
│   ├── ViewRenderer.{h,cpp}  — Ultralight bitmap → pixel buffer → D3D11 texture → SpriteBatch
│   ├── ViewOperationQueue.{h,cpp} — per-frame operation queue (max 100 ops/view)
│   ├── Communication.{h,cpp} — JS↔C++ bridge: Invoke, RegisterJSListener, InteropCall
│   ├── InputHandler.{h,cpp}  — WndProc hook, mouse/keyboard → Ultralight events
│   ├── GamepadInputHandler.{h,cpp} — Skyrim input events → Ultralight Gamepad API
│   ├── Listeners.{h,cpp}     — LoadListener / ViewListener (OnDOMReady, OnFinishLoading, console)
│   ├── Inspector.{h,cpp}     — DevTools as a separate Ultralight view rendered as overlay
│   ├── ImeHelper.{h,cpp}     — IME (CJK input) support
│   └── PrismaVR.{h,cpp}      — VR integration (OpenVR, 3D panels)
├── API/API.{h,cpp}           — IVPrismaUI1/IVPrismaUI2 implementation, exported via RequestPluginAPI
├── Hooks/Hooks.{h,cpp}       — D3DPresentHook
└── Utils/
    ├── SingleThreadExecutor.h — priority task queue on a single thread (the Ultralight thread)
    ├── NanoID.h               — PrismaViewId (uint64_t) generator
    └── WinKeyHandler/         — WinAPI VK codes → Ultralight KeyEvent
```

## Critical threading rules

`ultralightThread` (`SingleThreadExecutor`) is the **only** thread allowed to touch any `ultralight::View`, `Renderer`, or `JSContext`. **All** Ultralight calls must go through `ultralightThread.submit()`.

Per-frame flow in `D3DPresent`:

1. `ViewOperationQueue::ProcessAllViewOperations()` — drains queued Show/Hide/Focus ops
2. `ultralightThread.submit(Renderer::Update)` — JS ticks, layout, paint
3. `ultralightThread.submit(ViewRenderer::RenderViews)` — BitmapSurface → `pixelBuffer`
4. Game thread: `pixelBuffer` → D3D11 texture (Map/Unmap), SpriteBatch draw sorted by `view->order`

## Runtime and Address Library

PrismaUI does not link CommonLibSSE-NG. `Platform::SKSEHost` owns the raw SKSE function-table boundary, while
`Platform::RuntimeContext`, `AddressLibrary`, and `RelocationResolver` own runtime selection and address resolution.

Address Library is a required end-user dependency and is loaded from its standard `Data/SKSE/Plugins` filename. Do
not bundle address databases. Database availability does not imply ABI compatibility: every accepted runtime must map
to an explicit `AbiProfile`, and unknown profiles must fail before any hook or game-memory access.

Keep all Bethesda object offsets and engine calls inside `Platform::GameServices`. Application code must not introduce
`RE::*`, `REL::*`, or legacy SKSE `Game*.h` types.

## View lifecycle

```text
ViewManager::Create()   → PrismaView added to map; ultralightView = nullptr
  ↓  (per-frame on ultralightThread)
Renderer creates View   → ultralightView assigned
  ↓
OnDOMReady()            → domReadyCallback fires  ← safe to Invoke JS here
  ↓
OnFinishLoading()       → isLoadingFinished = true; BindJSCallbacks() called
```

`domReadyCallback` fires **before** `OnFinishLoading`. JS listeners bound by `RegisterJSListener` are only active after `OnFinishLoading`. They rebind automatically on every reload — no need to call `RegisterJSListener` again.

`IsValid()` only checks map membership; `ultralightView` may still be null. Always check `viewData->ultralightView` before use.

## JS ↔ C++ communication

```cpp
// C++ → JS (async, goes through ultralightThread queue)
Communication::Invoke(viewId, "fn('data')", [](std::string r) { /* result */ });

// C++ → JS (sync, call ONLY when already on ultralightThread)
Communication::InvokeFromUltralightThread(viewId, "fn()");

// JS → C++ (JS calls window.myCallback("data"))
Communication::RegisterJSListener(viewId, "myCallback", [](std::string arg) { });

// C++ → JS function call by name, single string arg — fastest path
Communication::InteropCall(viewId, "onData", jsonString);
```

## VR vs flatscreen differences

`Focus()` in flatscreen: captures the window input, disables game controls, and optionally pauses game.  
In VR (`PrismaVR::IsVRActive()`): none of that — player keeps full control. Always branch on this flag when touching focus-related logic.

Desktop mouse and keyboard events come from the window subclass. Gamepad events use a narrow, local representation of
Skyrim's input source so Steam Input and runtime remapping continue to work, then translate to Ultralight's standard
mapping. Scaleform FocusMenu/CursorMenu hooks are intentionally not part of the runtime ABI surface.

## HTML file resolution

```cpp
ViewManager::Create("ui/index.html");  // → file:///views/ui/index.html
ViewManager::Create("https://...");    // passed through as-is
```

## Other gotchas

- **Logging source locations:** `Platform::Logging` wrappers capture `std::source_location` and forward it as
  `spdlog::source_loc`. Do not replace them with direct `using spdlog::info` aliases; direct spdlog free functions
  leave `%s` and `%#` empty.
- **D3D hook publication:** prepare the call patch, release-publish the original target, then activate the patch.
  `D3DPresent` must acquire-load and null-check the target before calling it.
- **Pixel format:** Ultralight renders **BGRA** → D3D texture is `DXGI_FORMAT_B8G8R8A8_UNORM`. Do not swap to RGBA.
- **SEH translation** is installed on `ultralightThread`: access violations become `SEHException : std::exception`. Stack overflow is not translated — process will crash.
- **Inspector** is a second `ultralight::View` with its own texture and `inspectorBufferMutex`. Position set via `SetInspectorBounds`.
