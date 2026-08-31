#include "InputHandler.h"
#include "Platform/SKSEHost.h"

#include <commctrl.h>

#include "Communication.h"
#include "Core.h"
#include "GamepadInputHandler.h"
#include "ImeHelper.h"
#include "PrismaVR.h"
#include "Utils/Encoding.h"
#pragma comment(lib, "comctl32.lib")

namespace PrismaUI::InputHandler {
    using namespace Core;

    HWND g_hWnd = nullptr;
    SingleThreadExecutor* g_ultralightThreadExecutor = nullptr;
    std::map<Core::PrismaViewId, std::shared_ptr<Core::PrismaView>>* g_viewsMap = nullptr;
    std::shared_mutex* g_viewsMapMutex = nullptr;

    Core::PrismaViewId g_currentlyFocusedViewId;
    std::mutex g_focusedViewIdMutex;

    std::atomic<bool> g_isAnyInputCaptureActive = false;
    std::atomic<int> g_cursorX = 0;
    std::atomic<int> g_cursorY = 0;

    std::mutex g_eventQueueMutex;
    std::vector<InputEvent> g_eventQueue;

    // Clipboard safety limits
    constexpr size_t MAX_CLIPBOARD_SIZE = 1024 * 1024;  // 1MB max
    constexpr size_t MAX_CLIPBOARD_CHARS = 200000;      // 200K characters max

    wchar_t g_pendingHighSurrogate = 0;

    // WndProc subclass state
    static std::atomic<bool> g_wndProcInstalled = false;
    static constexpr UINT_PTR SUBCLASS_ID = 0x505249534D41;  // "PRISMA" in hex
    static std::mutex g_wndProcMutex;                        // Thread-safe installation

    static ImeHelper g_imeHelper;

    // Clipboard helper functions
    std::string EscapeForJS(const std::string& text) {
        std::string escaped;

        try {
            escaped.reserve(text.size() * 2);  // Reserve extra space for escape sequences
        } catch (const std::exception& e) {
            logger::error("Failed to allocate memory for escaped text: {}", e.what());
            return "";
        }

        for (size_t i = 0; i < text.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(text[i]);

            // Handle multi-byte UTF-8 sequences
            if (c >= 0x80) {
                // Check for Unicode line/paragraph separators (U+2028, U+2029)
                // U+2028 = E2 80 A8, U+2029 = E2 80 A9
                if (i + 2 < text.size() && c == 0xE2 && static_cast<unsigned char>(text[i + 1]) == 0x80 &&
                    (static_cast<unsigned char>(text[i + 2]) == 0xA8 ||
                     static_cast<unsigned char>(text[i + 2]) == 0xA9)) {
                    escaped += "\\u202";
                    escaped += (text[i + 2] == 0xA8) ? '8' : '9';
                    i += 2;
                    continue;
                }
                // Pass through other UTF-8 sequences
                escaped += c;
                continue;
            }

            // Handle special characters and control codes
            switch (c) {
                case '\'':
                    escaped += "\\'";
                    break;
                case '\"':
                    escaped += "\\\"";
                    break;
                case '\\':
                    escaped += "\\\\";
                    break;
                case '\n':
                    escaped += "\\n";
                    break;
                case '\r':
                    escaped += "\\r";
                    break;
                case '\t':
                    escaped += "\\t";
                    break;
                case '\b':
                    escaped += "\\b";
                    break;
                case '\f':
                    escaped += "\\f";
                    break;
                default:
                    // Filter out dangerous control characters (0x00-0x1F except handled above)
                    if (c < 0x20) {
                        // Skip null bytes and other control characters
                        logger::trace("Filtered control character: 0x{:02X}", static_cast<int>(c));
                    } else {
                        escaped += c;
                    }
                    break;
            }
        }
        return escaped;
    }

    std::string GetClipboardText() {
        if (!OpenClipboard(g_hWnd)) {
            logger::warn("Failed to open clipboard for reading");
            return "";
        }

        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (!hData) {
            CloseClipboard();
            return "";
        }

        wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
        if (!pszText) {
            CloseClipboard();
            return "";
        }

        // Check size before converting
        SIZE_T dataSize = GlobalSize(hData);
        if (dataSize > MAX_CLIPBOARD_SIZE) {
            logger::warn("Clipboard text too large: {} bytes (max: {} bytes). Truncating.", dataSize,
                         MAX_CLIPBOARD_SIZE);
            GlobalUnlock(hData);
            CloseClipboard();
            return "";
        }

        // Convert wide string to UTF-8
        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0) {
            GlobalUnlock(hData);
            CloseClipboard();
            return "";
        }

        // Check character count limit
        size_t charCount = wcslen(pszText);
        if (charCount > MAX_CLIPBOARD_CHARS) {
            logger::warn("Clipboard text too long: {} characters (max: {} characters). Truncating.", charCount,
                         MAX_CLIPBOARD_CHARS);
            GlobalUnlock(hData);
            CloseClipboard();
            return "";
        }

        std::string result;
        try {
            result.resize(utf8Length - 1);
            WideCharToMultiByte(CP_UTF8, 0, pszText, -1, &result[0], utf8Length, nullptr, nullptr);
        } catch (const std::exception& e) {
            logger::error("Failed to allocate memory for clipboard text: {}", e.what());
            GlobalUnlock(hData);
            CloseClipboard();
            return "";
        }

        GlobalUnlock(hData);
        CloseClipboard();

        return result;
    }

    void SetClipboardText(const std::string& text) {
        // Check size limits before processing
        if (text.size() > MAX_CLIPBOARD_SIZE) {
            logger::warn("Text too large to copy to clipboard: {} bytes (max: {} bytes)", text.size(),
                         MAX_CLIPBOARD_SIZE);
            return;
        }

        if (!OpenClipboard(g_hWnd)) {
            logger::warn("Failed to open clipboard for writing");
            return;
        }

        EmptyClipboard();

        // Convert UTF-8 to wide string
        int wideLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (wideLength <= 0) {
            CloseClipboard();
            return;
        }

        HGLOBAL hMem = nullptr;
        try {
            hMem = GlobalAlloc(GMEM_MOVEABLE, wideLength * sizeof(wchar_t));
            if (!hMem) {
                logger::error("Failed to allocate global memory for clipboard");
                CloseClipboard();
                return;
            }

            wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
            if (!pMem) {
                GlobalFree(hMem);
                CloseClipboard();
                return;
            }

            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pMem, wideLength);
            GlobalUnlock(hMem);

            if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
                GlobalFree(hMem);
                logger::warn("Failed to set clipboard data");
            }
        } catch (const std::exception& e) {
            logger::error("Exception while setting clipboard text: {}", e.what());
            if (hMem) {
                GlobalFree(hMem);
            }
        }

        CloseClipboard();
    }

    bool IsHighSurrogate(wchar_t ch) { return ch >= 0xD800 && ch <= 0xDBFF; }

    bool IsLowSurrogate(wchar_t ch) { return ch >= 0xDC00 && ch <= 0xDFFF; }

    bool ShouldQueueChar(wchar_t ch) { return ch >= 0x20 || ch == '\t'; }

    std::string ConvertUtf16ToUtf8(const wchar_t* text, int length) {
        if (!text || length <= 0) {
            return "";
        }

        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
        if (utf8Length <= 0) {
            return "";
        }

        std::string result;
        try {
            result.resize(utf8Length);
            WideCharToMultiByte(CP_UTF8, 0, text, length, result.data(), utf8Length, nullptr, nullptr);
        } catch (const std::exception& e) {
            logger::error("Failed to allocate memory for committed text: {}", e.what());
            return "";
        }

        return result;
    }

    void QueueCommittedCharEvent(const std::wstring& utf16Text, LPARAM lParam) {
        if (utf16Text.empty()) {
            return;
        }

        std::string utf8Text = ConvertUtf16ToUtf8(utf16Text.data(), static_cast<int>(utf16Text.size()));
        if (utf8Text.empty()) {
            logger::warn("Failed to convert committed text to UTF-8");
            return;
        }

        ultralight::KeyEvent charEvent;
        charEvent.type = ultralight::KeyEvent::kType_Char;
        WinKeyHandler::GetUltralightModifiers(charEvent);

        ultralight::String ulText = ultralight::Convert(utf8Text);
        charEvent.text = ulText;
        charEvent.unmodified_text = ulText;
        charEvent.virtual_key_code = ultralight::KeyCodes::GK_UNKNOWN;
        charEvent.native_key_code = 0;
        charEvent.key_identifier = "";
        charEvent.is_keypad = false;
        charEvent.is_auto_repeat = (HIWORD(lParam) & KF_REPEAT) == KF_REPEAT;
        charEvent.is_system_key = false;

        std::lock_guard lock(g_eventQueueMutex);
        g_eventQueue.emplace_back(charEvent);
    }

    std::wstring ConvertCodePointToUtf16(UINT codePoint) {
        if (codePoint > 0x10FFFF) {
            return L"";
        }

        if (codePoint <= 0xFFFF) {
            return std::wstring(1, static_cast<wchar_t>(codePoint));
        }

        codePoint -= 0x10000;
        wchar_t high = static_cast<wchar_t>(0xD800 + (codePoint >> 10));
        wchar_t low = static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF));

        return std::wstring{high, low};
    }

    void QueueMouseEvent(ultralight::MouseEvent::Type type, ultralight::MouseEvent::Button button, int x, int y) {
        g_cursorX.store(x, std::memory_order_relaxed);
        g_cursorY.store(y, std::memory_order_relaxed);
        ultralight::MouseEvent event;
        event.type = type;
        event.x = x;
        event.y = y;
        event.button = button;
        std::lock_guard lock(g_eventQueueMutex);
        g_eventQueue.emplace_back(event);
    }

    void QueueMouseWheel(HWND hwnd, WPARAM wParam, LPARAM lParam) {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hwnd, &point);
        g_cursorX.store(point.x, std::memory_order_relaxed);
        g_cursorY.store(point.y, std::memory_order_relaxed);

        int scrollPixelSize = 28;
        Core::PrismaViewId focusedViewId;
        {
            std::lock_guard lock(g_focusedViewIdMutex);
            focusedViewId = g_currentlyFocusedViewId;
        }
        if (focusedViewId != 0 && g_viewsMap && g_viewsMapMutex) {
            std::shared_ptr<Core::PrismaView> view;
            {
                std::shared_lock lock(*g_viewsMapMutex);
                const auto it = g_viewsMap->find(focusedViewId);
                if (it != g_viewsMap->end()) {
                    view = it->second;
                }
            }
            if (view) {
                scrollPixelSize = view->scrollingPixelSize;
            }
        }

        ScrollEventWithPosition event;
        event.event.type = ultralight::ScrollEvent::kType_ScrollByPixel;
        event.event.delta_x = 0;
        event.event.delta_y = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? scrollPixelSize : -scrollPixelSize;
        event.mouseX = point.x;
        event.mouseY = point.y;
        std::lock_guard lock(g_eventQueueMutex);
        g_eventQueue.emplace_back(event);
    }

    LRESULT CALLBACK SubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                  DWORD_PTR /*dwRefData*/) {
        // In VR, PrismaVR owns keyboard input; leave upstream desktop input handling untouched.
        if (PrismaVR::IsVRActive()) {
            if (uMsg == WM_NCDESTROY) {
                RemoveWindowSubclass(hwnd, SubclassProc, uIdSubclass);
                if (uIdSubclass == SUBCLASS_ID) {
                    g_wndProcInstalled.store(false);
                }
            }
            return DefSubclassProc(hwnd, uMsg, wParam, lParam);
        }

        LRESULT imeControlResult = 0;
        if (g_imeHelper.HandleControlMessage(hwnd, uMsg, wParam, lParam, &imeControlResult)) {
            return imeControlResult;
        }

        if (uMsg == WM_IME_SETCONTEXT) {
            LPARAM imeLParam = lParam;
            g_imeHelper.ModifySetContextLParam(&imeLParam, uMsg);
            lParam = imeLParam;
        }

        if (g_isAnyInputCaptureActive.load()) {
            bool handledByUI = false;
            Core::PrismaViewId focusedViewIdCopy;
            {
                std::lock_guard lock(g_focusedViewIdMutex);
                focusedViewIdCopy = g_currentlyFocusedViewId;
            }

            if (focusedViewIdCopy != 0) {
                if (g_imeHelper.HandleMessage(hwnd, uMsg, wParam, lParam, focusedViewIdCopy, &handledByUI)) {
                    if (handledByUI) {
                        return 0;
                    }
                }

                switch (uMsg) {
                    case WM_MOUSEMOVE:
                        QueueMouseEvent(ultralight::MouseEvent::kType_MouseMoved,
                                        ultralight::MouseEvent::kButton_None, GET_X_LPARAM(lParam),
                                        GET_Y_LPARAM(lParam));
                        handledByUI = true;
                        break;
                    case WM_LBUTTONDOWN:
                    case WM_RBUTTONDOWN:
                    case WM_MBUTTONDOWN: {
                        const auto button = uMsg == WM_LBUTTONDOWN ? ultralight::MouseEvent::kButton_Left
                                            : uMsg == WM_RBUTTONDOWN ? ultralight::MouseEvent::kButton_Right
                                                                    : ultralight::MouseEvent::kButton_Middle;
                        QueueMouseEvent(ultralight::MouseEvent::kType_MouseDown, button, GET_X_LPARAM(lParam),
                                        GET_Y_LPARAM(lParam));
                        handledByUI = true;
                        break;
                    }
                    case WM_LBUTTONUP:
                    case WM_RBUTTONUP:
                    case WM_MBUTTONUP: {
                        const auto button = uMsg == WM_LBUTTONUP ? ultralight::MouseEvent::kButton_Left
                                            : uMsg == WM_RBUTTONUP ? ultralight::MouseEvent::kButton_Right
                                                                  : ultralight::MouseEvent::kButton_Middle;
                        QueueMouseEvent(ultralight::MouseEvent::kType_MouseUp, button, GET_X_LPARAM(lParam),
                                        GET_Y_LPARAM(lParam));
                        handledByUI = true;
                        break;
                    }
                    case WM_MOUSEWHEEL:
                        QueueMouseWheel(hwnd, wParam, lParam);
                        handledByUI = true;
                        break;
                    case WM_KEYDOWN: {
                        // Handle Ctrl+V (Paste)
                        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'V') {
                            try {
                                std::string clipboardText = GetClipboardText();
                                if (!clipboardText.empty()) {
                                    logger::debug("Ctrl+V: Pasting {} characters from clipboard",
                                                  clipboardText.length());

                                    // Escape text for JavaScript string
                                    std::string escapedText = EscapeForJS(clipboardText);

                                    if (escapedText.empty() && !clipboardText.empty()) {
                                        logger::warn("Failed to escape clipboard text, paste cancelled");
                                    } else if (!escapedText.empty()) {
                                        // Use JavaScript execCommand to insert text properly (handles UTF-8)
                                        std::string script =
                                            "document.execCommand('insertText', false, '" + escapedText + "')";

                                        PrismaUI::Communication::Invoke(focusedViewIdCopy, script.c_str());
                                    }
                                }
                            } catch (const std::exception& e) {
                                logger::error("Exception during paste operation: {}", e.what());
                            }
                            handledByUI = true;
                            break;
                        }

                        // Handle Ctrl+C (Copy)
                        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'C') {
                            // Execute JavaScript to get selected text on ultralightThread
                            if (g_ultralightThreadExecutor && g_viewsMap && g_viewsMapMutex) {
                                g_ultralightThreadExecutor->submit([viewId = focusedViewIdCopy]() {
                                    try {
                                        std::shared_ptr<Core::PrismaView> viewData = nullptr;
                                        {
                                            std::shared_lock lock(*g_viewsMapMutex);
                                            auto it = g_viewsMap->find(viewId);
                                            if (it != g_viewsMap->end()) {
                                                viewData = it->second;
                                            }
                                        }

                                        if (viewData && viewData->ultralightView) {
                                            ultralight::String script = "window.getSelection().toString()";
                                            ultralight::String result =
                                                viewData->ultralightView->EvaluateScript(script, nullptr, "");
                                            std::string selectedText = result.utf8().data();

                                            // Check size before copying
                                            if (selectedText.size() > MAX_CLIPBOARD_SIZE) {
                                                logger::warn(
                                                    "Selected text too large to copy: {} bytes (max: {} bytes)",
                                                    selectedText.size(), MAX_CLIPBOARD_SIZE);
                                                return;
                                            }

                                            if (!selectedText.empty()) {
                                                // Copy to clipboard on Skyrim main thread to avoid blocking
                                                Platform::SKSEHost::AddTask([text = selectedText]() {
                                                    SetClipboardText(text);
                                                    logger::debug("Ctrl+C: Copied {} characters to clipboard",
                                                                  text.length());
                                                });
                                            }
                                        }
                                    } catch (const std::exception& e) {
                                        logger::error("Exception during copy operation: {}", e.what());
                                    }
                                });
                            }
                            handledByUI = true;
                            break;
                        }

                        // Normal key processing
                        ultralight::KeyEvent keyDownEvent =
                            WinKeyHandler::CreateKeyEvent(ultralight::KeyEvent::kType_RawKeyDown, wParam, lParam);
                        {
                            std::lock_guard lock(g_eventQueueMutex);
                            g_eventQueue.emplace_back(keyDownEvent);
                        }
                        handledByUI = true;
                        break;
                    }
                    case WM_KEYUP: {
                        ultralight::KeyEvent ev =
                            WinKeyHandler::CreateKeyEvent(ultralight::KeyEvent::kType_KeyUp, wParam, lParam);
                        {
                            std::lock_guard lock(g_eventQueueMutex);
                            g_eventQueue.emplace_back(ev);
                        }
                        handledByUI = true;
                        break;
                    }
                    case WM_CHAR: {
                        handledByUI = true;

                        wchar_t ch = static_cast<wchar_t>(wParam);
                        if (IsHighSurrogate(ch)) {
                            g_pendingHighSurrogate = ch;
                            break;
                        }

                        std::wstring committedText;
                        if (IsLowSurrogate(ch) && g_pendingHighSurrogate != 0) {
                            committedText.push_back(g_pendingHighSurrogate);
                            committedText.push_back(ch);
                            g_pendingHighSurrogate = 0;
                        } else {
                            g_pendingHighSurrogate = 0;
                            if (ShouldQueueChar(ch)) {
                                committedText.push_back(ch);
                            }
                        }

                        QueueCommittedCharEvent(committedText, lParam);
                        break;
                    }
                    case WM_UNICHAR: {
                        if (wParam == UNICODE_NOCHAR) {
                            return TRUE;
                        }

                        handledByUI = true;

                        std::wstring committedText = ConvertCodePointToUtf16(static_cast<UINT>(wParam));
                        if (!committedText.empty() && ShouldQueueChar(committedText[0])) {
                            g_pendingHighSurrogate = 0;
                            QueueCommittedCharEvent(committedText, lParam);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }

            if (handledByUI) {
                return 0;
            }
        }

        // Handle window destruction - clean up subclass
        if (uMsg == WM_NCDESTROY) {
            RemoveWindowSubclass(hwnd, SubclassProc, uIdSubclass);
            if (uIdSubclass == SUBCLASS_ID) {
                g_wndProcInstalled.store(false);
            }
        }

        // Pass to next handler in the chain using DefSubclassProc
        // This properly handles the chain even if other mods are in the stack
        return DefSubclassProc(hwnd, uMsg, wParam, lParam);
    }

    void Initialize(HWND gameHwnd, SingleThreadExecutor* coreExecutor,
                    std::map<Core::PrismaViewId, std::shared_ptr<Core::PrismaView>>* viewsMap,
                    std::shared_mutex* viewsMapMutex) {
        g_hWnd = gameHwnd;
        g_ultralightThreadExecutor = coreExecutor;
        g_viewsMap = viewsMap;
        g_viewsMapMutex = viewsMapMutex;
        g_isAnyInputCaptureActive = false;
        POINT cursorPosition{};
        if (GetCursorPos(&cursorPosition) && ScreenToClient(g_hWnd, &cursorPosition)) {
            g_cursorX.store(cursorPosition.x, std::memory_order_relaxed);
            g_cursorY.store(cursorPosition.y, std::memory_order_relaxed);
        }
        {
            std::lock_guard lock(g_focusedViewIdMutex);
            g_currentlyFocusedViewId = 0;
        }

        logger::info("PrismaUI::InputHandler Initialized with HWND: {}", (void*)g_hWnd);

        g_imeHelper.SetCallbacks(
            [](const std::string& s) { return EscapeForJS(s); },
            [](const std::wstring& ws, LPARAM lp) { QueueCommittedCharEvent(ws, lp); },
            [](const wchar_t* p, int len) { return ConvertUtf16ToUtf8(p, len); });
        g_imeHelper.SetContext({g_hWnd, g_viewsMap, g_viewsMapMutex});
        g_imeHelper.SetExecutor(g_ultralightThreadExecutor);
        g_imeHelper.Initialize(g_hWnd);

        GamepadInputHandler::Initialize(g_ultralightThreadExecutor);
    }

    bool InstallWndProcHook() {
        std::lock_guard<std::mutex> lock(g_wndProcMutex);

        if (g_wndProcInstalled.load()) {
            logger::debug("WndProc subclass already installed");
            return true;
        }

        if (!g_hWnd) {
            logger::error("Cannot install WndProc subclass: HWND is null");
            return false;
        }

        // Check if HWND is valid
        if (!IsWindow(g_hWnd)) {
            logger::error("HWND {:p} is not a valid window", (void*)g_hWnd);
            return false;
        }

        logger::debug("Attempting to install subclass on HWND: {:p}", (void*)g_hWnd);

        // Clear last error before calling
        SetLastError(0);

        if (!SetWindowSubclass(g_hWnd, SubclassProc, SUBCLASS_ID, 0)) {
            DWORD err = GetLastError();
            logger::error("Failed to install WndProc subclass. Error: {} (0x{:X})", err, err);
            return false;
        }

        g_wndProcInstalled.store(true);
        logger::info("WndProc subclass installed successfully on HWND: {:p}", (void*)g_hWnd);
        return true;
    }

    void UninstallWndProcHook() {
        std::lock_guard<std::mutex> lock(g_wndProcMutex);

        if (!g_wndProcInstalled.exchange(false)) {
            return;
        }

        if (g_hWnd) {
            RemoveWindowSubclass(g_hWnd, SubclassProc, SUBCLASS_ID);
            logger::info("WndProc subclass removed");
        }
    }

    void EnableInputCapture(const Core::PrismaViewId& viewId) {
        if (viewId == 0) {
            logger::warn("EnableInputCapture called with empty viewId.");
            return;
        }
        {
            std::lock_guard lock(g_focusedViewIdMutex);
            if (g_currentlyFocusedViewId != viewId) {
                g_currentlyFocusedViewId = viewId;
                logger::debug("PrismaUI Input Capture focused on View [{}].", viewId);
            }
        }

        if (!g_isAnyInputCaptureActive.exchange(true)) {
            logger::debug("PrismaUI Input Capture System Enabled for View [{}].", viewId);
        }

        g_imeHelper.SetAssociation(true);

        //A view just gained focus. Clear stale button values so a held button re-fires on its next press:
        GamepadInputHandler::ResetButtonValues();
    }

    void DisableInputCapture(const Core::PrismaViewId& viewIdToUnfocus) {
        bool disableSystem = false;
        Core::PrismaViewId currentFocusedBeforeDisable;
        {
            std::lock_guard lock(g_focusedViewIdMutex);
            currentFocusedBeforeDisable = g_currentlyFocusedViewId;
            if (viewIdToUnfocus == 0 || viewIdToUnfocus == g_currentlyFocusedViewId) {
                if (g_isAnyInputCaptureActive.load()) {
                    disableSystem = true;
                    g_currentlyFocusedViewId = 0;
                }
            }
        }

        if (disableSystem) {
            if (g_isAnyInputCaptureActive.exchange(false)) {
                g_imeHelper.SetAssociation(false);
                logger::debug("PrismaUI Input Capture System Disabled (was active for View [{}]).",
                              currentFocusedBeforeDisable);

                //Focus released. Reset button values so it doesn't leak into the next focused view.
                GamepadInputHandler::ResetButtonValues();

                if (g_ultralightThreadExecutor && currentFocusedBeforeDisable != 0) {
                    g_ultralightThreadExecutor->submit([viewId_copy = currentFocusedBeforeDisable]() {
                        std::shared_ptr<Core::PrismaView> targetViewData = nullptr;
                        {
                            std::shared_lock lock(*g_viewsMapMutex);
                            auto it = g_viewsMap->find(viewId_copy);
                            if (it != g_viewsMap->end()) {
                                targetViewData = it->second;
                            }
                        }

                        if (targetViewData && targetViewData->ultralightView) {
                            logger::debug("Resetting mouse position to (0,0) for View [{}]", viewId_copy);
                            ultralight::MouseEvent resetEvent;
                            resetEvent.type = ultralight::MouseEvent::kType_MouseMoved;
                            resetEvent.x = 0;
                            resetEvent.y = 0;
                            resetEvent.button = ultralight::MouseEvent::kButton_None;

                            targetViewData->ultralightView->FireMouseEvent(resetEvent);
                        }
                    });
                }
            }
        } else if (viewIdToUnfocus != 0) {
            logger::debug(
                "PrismaUI: DisableInputCapture called for View [{}] but View [{}] is/was focused. No change to system "
                "state, only unfocused ID removed if it matched.",
                viewIdToUnfocus, currentFocusedBeforeDisable);
        }
    }

    void ClearImeState(const Core::PrismaViewId& viewId) {
        if (viewId == 0) {
            return;
        }

        g_imeHelper.SetAssociation(false);
        g_imeHelper.ClearStateInJS(viewId);
    }

    bool IsAnyInputCaptureActive() { return g_isAnyInputCaptureActive.load(); }

    Core::PrismaViewId GetFocusedViewId() {
        std::lock_guard lock(g_focusedViewIdMutex);
        return g_currentlyFocusedViewId;
    }

    CursorPosition GetCursorPosition() noexcept {
        return {g_cursorX.load(std::memory_order_relaxed), g_cursorY.load(std::memory_order_relaxed)};
    }

    bool IsInputCaptureActiveForView(const Core::PrismaViewId& viewId) {
        Core::PrismaViewId currentFocused;
        {
            std::lock_guard lock(g_focusedViewIdMutex);
            currentFocused = g_currentlyFocusedViewId;
        }
        if (viewId == 0) return g_isAnyInputCaptureActive.load();
        return g_isAnyInputCaptureActive.load() && (currentFocused == viewId);
    }

    void ProcessEvents() {
        if (!g_ultralightThreadExecutor || !g_viewsMap || !g_viewsMapMutex) return;

        GamepadInputHandler::ProcessEvents();

        Core::PrismaViewId focusedViewIdCopy;
        {
            std::lock_guard lock(g_focusedViewIdMutex);
            focusedViewIdCopy = g_currentlyFocusedViewId;
        }

        if (focusedViewIdCopy == 0 && !g_eventQueue.empty()) {
            std::lock_guard lock(g_eventQueueMutex);
            g_eventQueue.clear();
            return;
        }
        if (focusedViewIdCopy == 0) return;

        std::vector<InputEvent> eventsToProcess;
        {
            std::lock_guard lock(g_eventQueueMutex);
            if (g_eventQueue.empty()) return;
            eventsToProcess.swap(g_eventQueue);
        }

        g_ultralightThreadExecutor->submit([viewId_copy = focusedViewIdCopy, ev_queue = std::move(eventsToProcess)]() {
            std::shared_ptr<Core::PrismaView> targetViewData = nullptr;
            {
                std::shared_lock lock(*g_viewsMapMutex);
                auto it = g_viewsMap->find(viewId_copy);
                if (it != g_viewsMap->end()) {
                    targetViewData = it->second;
                }
            }

            if (targetViewData && targetViewData->ultralightView) {
                ultralight::View* ulView = targetViewData->ultralightView.get();
                ultralight::View* inspectorView =
                    targetViewData->inspectorView ? targetViewData->inspectorView.get() : nullptr;

                for (const auto& event_variant : ev_queue) {
                    std::visit(
                        [ulView, inspectorView, &targetViewData](const auto& arg) {
                            using T = std::decay_t<decltype(arg)>;
                            if constexpr (std::is_same_v<T, ultralight::MouseEvent>) {
                                // Check if mouse is over inspector bounds when inspector is visible
                                bool mouseOverInspector = false;
                                if (inspectorView && targetViewData->inspectorVisible.load()) {
                                    const float inspX = targetViewData->inspectorPosX;
                                    const float inspY = targetViewData->inspectorPosY;
                                    const float inspW = static_cast<float>(targetViewData->inspectorDisplayWidth);
                                    const float inspH = static_cast<float>(targetViewData->inspectorDisplayHeight);

                                    const float mouseX = static_cast<float>(arg.x);
                                    const float mouseY = static_cast<float>(arg.y);

                                    if (mouseX >= inspX && mouseX < (inspX + inspW) && mouseY >= inspY &&
                                        mouseY < (inspY + inspH)) {
                                        mouseOverInspector = true;
                                        targetViewData->inspectorPointerHover.store(true);
                                    } else {
                                        targetViewData->inspectorPointerHover.store(false);
                                    }
                                }

                                if (mouseOverInspector) {
                                    // Translate mouse coordinates to inspector view
                                    ultralight::MouseEvent inspectorEvent = arg;
                                    inspectorEvent.x = arg.x - static_cast<int>(targetViewData->inspectorPosX);
                                    inspectorEvent.y = arg.y - static_cast<int>(targetViewData->inspectorPosY);
                                    inspectorView->FireMouseEvent(inspectorEvent);
                                } else {
                                    ulView->FireMouseEvent(arg);
                                }
                            } else if constexpr (std::is_same_v<T, ScrollEventWithPosition>) {
                                // Route scroll events to inspector if mouse is over it
                                // Use captured mouse position for accurate bounds checking
                                bool scrollOverInspector = false;
                                if (inspectorView && targetViewData->inspectorVisible.load()) {
                                    const float inspX = targetViewData->inspectorPosX;
                                    const float inspY = targetViewData->inspectorPosY;
                                    const float inspW = static_cast<float>(targetViewData->inspectorDisplayWidth);
                                    const float inspH = static_cast<float>(targetViewData->inspectorDisplayHeight);

                                    const float mouseX = static_cast<float>(arg.mouseX);
                                    const float mouseY = static_cast<float>(arg.mouseY);

                                    if (mouseX >= inspX && mouseX < (inspX + inspW) && mouseY >= inspY &&
                                        mouseY < (inspY + inspH)) {
                                        scrollOverInspector = true;
                                    }
                                }
                                if (scrollOverInspector) {
                                    inspectorView->FireScrollEvent(arg.event);
                                } else {
                                    ulView->FireScrollEvent(arg.event);
                                }
                            } else if constexpr (std::is_same_v<T, ultralight::KeyEvent>) {
                                // Route keyboard events to inspector if it's visible and focused
                                if (inspectorView && targetViewData->inspectorVisible.load() &&
                                    inspectorView->HasFocus()) {
                                    inspectorView->FireKeyEvent(arg);
                                } else {
                                    ulView->FireKeyEvent(arg);
                                }
                            }
                        },
                        event_variant);
                }
            }
        });
    }

    void Shutdown() {
        DisableInputCapture(0);
        {
            std::lock_guard lock(g_eventQueueMutex);
            g_eventQueue.clear();
        }

        GamepadInputHandler::Shutdown();

        UninstallWndProcHook();

        g_imeHelper.Shutdown(g_hWnd);

        g_hWnd = nullptr;
        g_ultralightThreadExecutor = nullptr;
        g_viewsMap = nullptr;
        g_viewsMapMutex = nullptr;
        logger::info("PrismaUI::InputHandler Shutdown.");
    }
}
