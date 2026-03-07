#include "Listeners.h"

#include "Communication.h"
#include "Core.h"
#include "PrismaUI_API.h"
#include "Audio/AudioBridge.h"
#include "Audio/AudioShim.h"
#include "Stubs/WebAudioStub.h"
#include "WASM/WASMBridge.h"
#include "WebGL/WebGLBridge.h"
#include "WebGL/WebGLShim.h"

namespace PrismaUI::Listeners {
    using namespace Core;
    using namespace Communication;

    // Escape a C++ string for safe embedding inside a JS single-quoted string literal.
    static std::string EscapeForJSString(const std::string& s) {
        std::string result;
        result.reserve(s.size() + 64);
        for (char c : s) {
            switch (c) {
                case '\\': result += "\\\\"; break;
                case '\'': result += "\\'"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                case '\0': result += "\\0"; break;
                default:   result += c; break;
            }
        }
        return result;
    }

    // Helper: inject browser API stubs + WebGL shim + native bindings into the main frame.
    // Also stores the shim source on window.__prismaShimSource for iframe propagation.
    // WebGL and WASM bindings are only injected for accelerated views (CreateViewAccelerated).
    static void InjectWebGLIntoMainFrame(View* caller, Core::PrismaViewId viewId) {
        bool isAccelerated = false;
        {
            std::shared_lock lock(Core::viewsMutex);
            auto it = Core::views.find(viewId);
            if (it != Core::views.end())
                isAccelerated = it->second->isAccelerated;
        }

        // Inject Web Audio shim (replaces the old no-op stub)
        const auto& audioShimJS = Audio::GetAudioShimJS();
        if (!audioShimJS.empty()) {
            caller->EvaluateScript(String(audioShimJS.c_str()), nullptr, String(""));

            // Store audio shim source on window for subframe propagation
            std::string escapedAudio = EscapeForJSString(audioShimJS);
            std::string storeAudioScript = "window.__prismaAudioShimSource = '" + escapedAudio + "';";
            caller->EvaluateScript(String(storeAudioScript.c_str()), nullptr, String(""));
        } else {
            // Fallback to no-op stub if shim file not found
            const char* audioStubJS = Stubs::GetWebAudioStubJS();
            if (audioStubJS && audioStubJS[0]) {
                caller->EvaluateScript(String(audioStubJS), nullptr, String(""));
            }
        }

        if (isAccelerated) {
            const auto& shimJS = WebGL::GetShimJS();
            if (!shimJS.empty()) {
                caller->EvaluateScript(String(shimJS.c_str()), nullptr, String(""));

                // Store shim source on window so subframe injection can access it.
                std::string escaped = EscapeForJSString(shimJS);
                std::string storeScript = "window.__prismaShimSource = '" + escaped + "';";
                caller->EvaluateScript(String(storeScript.c_str()), nullptr, String(""));
            }
        }

        auto scoped_context = caller->LockJSContext(String(""));
        if (scoped_context) {
            JSContextRef ctx = (*scoped_context);
            if (isAccelerated) {
                WebGL::InjectWebGLBindings(ctx, viewId);
                WASM::InjectWASMBindings(ctx, viewId);
            }
            Audio::InjectAudioBindings(ctx, viewId);
        }
    }

    // MyLoadListener
    MyLoadListener::MyLoadListener(Core::PrismaViewId id) : viewId_(std::move(id)) {}

    MyLoadListener::~MyLoadListener() = default;

    void MyLoadListener::OnBeginLoading(View* /*caller*/, uint64_t /*frame_id*/, bool is_main_frame,
                                        const String& url) {
        logger::info("View [{}]: LoadListener: Begin loading URL: {}", viewId_, url.utf8().data());
        if (is_main_frame) {
            webglInjectedForLoad_ = false;
        }
    }

    void MyLoadListener::OnFinishLoading(View* caller, uint64_t /*frame_id*/, bool is_main_frame,
                                         const String& url) {
        logger::info("View [{}]: LoadListener: Finished loading URL: {}", viewId_, url.utf8().data());

        // Fallback: if OnWindowObjectReady didn't fire for this page load
        // (Ultralight skips it for in-view navigations), inject WebGL shim + bindings now.
        if (is_main_frame && !webglInjectedForLoad_) {
            logger::info("View [{}]: OnWindowObjectReady was not called; injecting WebGL bindings in OnFinishLoading.", viewId_);
            InjectWebGLIntoMainFrame(caller, viewId_);
            webglInjectedForLoad_ = true;
        }

        ultralightThread.submit([id = viewId_, urlStr = std::string(url.utf8().data())] {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(id);
            if (it != views.end()) {
                it->second->isLoadingFinished = true;
                it->second->lastLoadedUrl = urlStr;
                it->second->recoveryAttempts = 0;
                Communication::BindJSCallbacks(id);
            }
        });
    }

    void MyLoadListener::OnFailLoading(View* /*caller*/, uint64_t /*frame_id*/, bool /*is_main_frame*/,
                                       const String& url, const String& description, const String& /*error_domain*/,
                                       int /*error_code*/) {
        logger::error("View [{}]: LoadListener: Failed loading URL: {}. Error: {}", viewId_, url.utf8().data(),
                      description.utf8().data());
        ultralightThread.submit([id = viewId_] {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(id);
            if (it != views.end()) {
                it->second->isLoadingFinished = false;
            }
        });
    }

    // Helper: inject WebGL shim into a subframe by recursively searching all
    // iframes from the main frame's context.  Runs JS in the main frame (empty
    // frame name — always works) which walks the iframe tree, matches by URL,
    // copies native bridge functions, computes accumulated iframe offsets, and
    // evals the shim source in the target frame's contentWindow.
    static bool InjectWebGLIntoSubframeViaMainFrame(View* caller, Core::PrismaViewId viewId,
                                                    const std::string& targetUrl) {
        std::string escapedUrl = EscapeForJSString(targetUrl);

        std::string script = R"JS(
            (function() {
                var targetUrl = ')JS" + escapedUrl + R"JS(';
                var shimSrc = window.__prismaShimSource;
                if (!shimSrc || typeof __prismaCreateWebGLContext !== 'function')
                    return 'no_shim';
                function search(doc, offX, offY) {
                    var iframes = doc.querySelectorAll('iframe');
                    for (var i = 0; i < iframes.length; i++) {
                        try {
                            var cw = iframes[i].contentWindow;
                            if (!cw) continue;
                            var fr = iframes[i].getBoundingClientRect();
                            var nx = offX + (fr.left || 0);
                            var ny = offY + (fr.top || 0);
                            var loc = '';
                            try { loc = cw.location.href; } catch(e) { continue; }
                            var locNorm = loc, tgtNorm = targetUrl;
                            try { locNorm = new URL(loc).href; } catch(e) {}
                            try { tgtNorm = new URL(targetUrl).href; } catch(e) {}
                            if (locNorm === tgtNorm) {
                                if (typeof cw.__prismaWebGLShimLoaded !== 'undefined')
                                    return 'already';
                                cw.__prismaCreateWebGLContext = __prismaCreateWebGLContext;
                                cw.__prismaUpdateWebGLContext = __prismaUpdateWebGLContext;
                                cw.__prismaShimSource = shimSrc;
                                cw.__prismaFrameOffsetX = nx;
                                cw.__prismaFrameOffsetY = ny;
                                if (typeof WebAssembly !== 'undefined') {
                                    cw.WebAssembly = WebAssembly;
                                    if (typeof __prismaWASMResolve === 'function')
                                        cw.__prismaWASMResolve = __prismaWASMResolve;
                                    if (typeof __prismaWASMReject === 'function')
                                        cw.__prismaWASMReject = __prismaWASMReject;
                                }
                                if (typeof AudioContext !== 'undefined')
                                    cw.AudioContext = AudioContext;
                                if (typeof webkitAudioContext !== 'undefined')
                                    cw.webkitAudioContext = webkitAudioContext;
                                if (typeof Audio !== 'undefined')
                                    cw.Audio = Audio;
                                if (typeof __prismaCreateAudioContext === 'function')
                                    cw.__prismaCreateAudioContext = __prismaCreateAudioContext;
                                cw.eval(shimSrc);
                                return 'injected';
                            }
                            if (cw.document) {
                                var r = search(cw.document, nx, ny);
                                if (r === 'injected' || r === 'already') return r;
                            }
                        } catch(e) {}
                    }
                    return 'not_found';
                }
                return search(document, 0, 0);
            })()
        )JS";

        String exception;
        String result = caller->EvaluateScript(String(script.c_str()), &exception, String(""));

        if (!exception.empty()) {
            logger::warn("View [{}]: Recursive subframe inject script failed: {}", viewId,
                         exception.utf8().data());
            return false;
        }

        std::string resultStr(result.utf8().data(), result.utf8().length());
        logger::info("View [{}]: Recursive subframe inject for '{}' => {}", viewId,
                     targetUrl, resultStr);

        return resultStr == "injected" || resultStr == "already";
    }

    void MyLoadListener::OnWindowObjectReady(View* caller, uint64_t /*frame_id*/, bool is_main_frame,
                                             const String& url) {
        if (is_main_frame) {
            logger::info("View [{}]: LoadListener: Window object ready.", viewId_);

            InjectWebGLIntoMainFrame(caller, viewId_);
            webglInjectedForLoad_ = true;
        } else {
            // Subframe: inject WebGL shim by running JS in the main frame that
            // recursively searches all iframes for the matching URL and injects
            // via contentWindow.eval().  No hardcoded frame names needed.
            std::string urlStr(url.utf8().data(), url.utf8().length());

            // Skip about:blank and about:srcdoc — these are iframe placeholders
            // that fire OnWindowObjectReady but don't need WebGL injection.
            if (urlStr == "about:blank" || urlStr == "about:srcdoc") {
                logger::info("View [{}]: LoadListener: Skipping subframe placeholder: {}", viewId_, urlStr);
                return;
            }

            logger::info("View [{}]: LoadListener: Subframe window object ready, URL: {}", viewId_, urlStr);

            bool isAccelerated = false;
            {
                std::shared_lock lock(Core::viewsMutex);
                auto it = Core::views.find(viewId_);
                if (it != Core::views.end())
                    isAccelerated = it->second->isAccelerated;
            }
            if (isAccelerated && !InjectWebGLIntoSubframeViaMainFrame(caller, viewId_, urlStr)) {
                logger::warn("View [{}]: Could not inject into subframe URL: {}", viewId_, urlStr);
            }
        }
    }

    void MyLoadListener::OnDOMReady(View* /*caller*/, uint64_t /*frame_id*/, bool is_main_frame,
                                    const String& /*url*/) {
        if (is_main_frame) {
            logger::info("View [{}]: LoadListener: DOM ready.", viewId_);

            ultralightThread.submit([id = viewId_] {
                std::shared_lock lock(viewsMutex);
                auto it = views.find(id);
                if (it != views.end() && it->second->domReadyCallback) {
                    it->second->domReadyCallback(id);
                }
            });
        }
    }

    // MyViewListener
    MyViewListener::MyViewListener(Core::PrismaViewId id) : viewId_(std::move(id)) {}

    MyViewListener::~MyViewListener() = default;

    void MyViewListener::OnAddConsoleMessage([[maybe_unused]] View* caller, [[maybe_unused]] const ConsoleMessage& message) {
        auto callerUrl = caller ? caller->url().utf8().data() : "unknown";
        logger::info("View [{}] on {}: JSConsole: {}", viewId_, callerUrl, message.message().utf8().data());

        std::shared_lock lock(viewsMutex);
        auto it = views.find(viewId_);
        if (it != views.end() && it->second && it->second->consoleMessageCallback) {
            PRISMA_UI_API::ConsoleMessageLevel level = PRISMA_UI_API::ConsoleMessageLevel::Log;
            switch (message.level()) {
                case kMessageLevel_Warning: level = PRISMA_UI_API::ConsoleMessageLevel::Warning; break;
                case kMessageLevel_Error:   level = PRISMA_UI_API::ConsoleMessageLevel::Error; break;
                case kMessageLevel_Debug:   level = PRISMA_UI_API::ConsoleMessageLevel::Debug; break;
                case kMessageLevel_Info:    level = PRISMA_UI_API::ConsoleMessageLevel::Info; break;
                default: break;
            }
            auto msg = std::string(message.message().utf8().data());
            auto cb = it->second->consoleMessageCallback;
            auto id = viewId_;
            lock.unlock();
            cb(id, level, msg);
        }
    }

    RefPtr<View> MyViewListener::OnCreateInspectorView([[maybe_unused]] View* caller, bool is_local, const String& inspectedURL) {
        logger::info(
            "View [{}]: ViewListener: OnCreateInspectorView called (is_local={}, "
            "URL={})",
            viewId_, is_local, inspectedURL.utf8().data());

        RefPtr<View> inspectorView = nullptr;

        std::unique_lock lock(viewsMutex);
        auto it = views.find(viewId_);
        if (it != views.end() && it->second) {
            auto viewData = it->second;

            if (!viewData->inspectorView && viewData->ultralightView && renderer) {
                uint32_t width = viewData->inspectorDisplayWidth.load() > 0 ? viewData->inspectorDisplayWidth.load() : 800;
                uint32_t height = viewData->inspectorDisplayHeight.load() > 0 ? viewData->inspectorDisplayHeight.load() : 600;

                ViewConfig config;
                config.is_accelerated = false;
                config.is_transparent = true;

                viewData->inspectorView = renderer->CreateView(width, height, config, nullptr);
                inspectorView = viewData->inspectorView;

                logger::info("View [{}]: Inspector view created with size {}x{}", viewId_, width, height);
            } else if (viewData->inspectorView) {
                inspectorView = viewData->inspectorView;
                logger::info("View [{}]: Returning existing inspector view", viewId_);
            }
        }

        return inspectorView;
    }

    // MyUltralightLogger
    MyUltralightLogger::~MyUltralightLogger() = default;

    void MyUltralightLogger::LogMessage(LogLevel /*log_level*/, const String& /*message*/) {
        // Implementation was empty, so keep it empty.
    }
}  // namespace PrismaUI::Listeners
