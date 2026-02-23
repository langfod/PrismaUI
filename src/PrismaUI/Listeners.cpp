#include "Listeners.h"

#include "Communication.h"
#include "Core.h"
#include "PrismaUI_API.h"
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

    // Helper: inject WebGL shim + native bindings into the main frame.
    // Also stores the shim source on window.__prismaShimSource for iframe propagation.
    static void InjectWebGLIntoMainFrame(View* caller, Core::PrismaViewId viewId) {
        const auto& shimJS = WebGL::GetShimJS();
        if (!shimJS.empty()) {
            caller->EvaluateScript(String(shimJS.c_str()), nullptr, String(""));

            // Store shim source on window so subframe injection can access it.
            std::string escaped = EscapeForJSString(shimJS);
            std::string storeScript = "window.__prismaShimSource = '" + escaped + "';";
            caller->EvaluateScript(String(storeScript.c_str()), nullptr, String(""));
        }

        auto scoped_context = caller->LockJSContext(String(""));
        if (scoped_context) {
            JSContextRef ctx = (*scoped_context);
            WebGL::InjectWebGLBindings(ctx, viewId);
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

    void MyLoadListener::OnWindowObjectReady(View* caller, uint64_t /*frame_id*/, bool is_main_frame,
                                             const String& /*url*/) {
        if (is_main_frame) {
            logger::info("View [{}]: LoadListener: Window object ready.", viewId_);

            InjectWebGLIntoMainFrame(caller, viewId_);
            webglInjectedForLoad_ = true;
        } else {
            // Subframe: LockJSContext(frameName) and EvaluateScript(script, exc, frameName)
            // do NOT work with dynamically-set iframe name attributes in Ultralight.
            // Instead, inject WebGL into iframes via cross-frame contentWindow access
            // from the main frame's JS context.
            logger::info("View [{}]: LoadListener: Subframe window object ready.", viewId_);

            String result = caller->EvaluateScript(String(R"JS(
                (function() {
                    var shimSrc = window.__prismaShimSource;
                    if (!shimSrc || typeof window.__prismaCreateWebGLContext !== 'function') {
                        return 'no_shim_or_bridge';
                    }
                    var injected = 0;
                    var iframes = document.querySelectorAll('iframe');
                    for (var i = 0; i < iframes.length; i++) {
                        try {
                            var win = iframes[i].contentWindow;
                            if (!win || win.__prismaWebGLShimLoaded) continue;
                            // Skip about:blank iframes — they inherit the parent's CSP
                            // (which blocks eval) and don't need WebGL anyway.
                            var src = iframes[i].getAttribute('src') || '';
                            if (!src || src === 'about:blank') continue;
                            // Copy native bridge functions to iframe's window
                            win.__prismaCreateWebGLContext = window.__prismaCreateWebGLContext;
                            win.__prismaUpdateWebGLContext = window.__prismaUpdateWebGLContext;
                            // Execute shim in the iframe's JS context
                            win.eval(shimSrc);
                            injected++;
                        } catch(e) {
                            console.log('[PrismaUI] iframe WebGL injection error: ' + e);
                        }
                    }
                    return 'injected:' + injected;
                })()
            )JS"), nullptr, String(""));

            logger::info("View [{}]: Subframe WebGL injection result: {}", viewId_,
                          std::string(result.utf8().data(), result.utf8().length()));
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

    void MyViewListener::OnAddConsoleMessage(View* /*caller*/, const ConsoleMessage& message) {
        // logger::info("View [{}]: JSConsole: {}", viewId_, message.message().utf8().data());
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

    RefPtr<View> MyViewListener::OnCreateInspectorView(View* /*caller*/, bool is_local, const String& inspectedURL) {
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
                uint32_t width = viewData->inspectorDisplayWidth > 0 ? viewData->inspectorDisplayWidth : 800;
                uint32_t height = viewData->inspectorDisplayHeight > 0 ? viewData->inspectorDisplayHeight : 600;

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
