// TokenStatus: tiny, dependency-free native Notepad++ plugin.
// Windows build: use Windows SDK headers. Portable CI cross-build: -DTOKENSTATUS_HEADERLESS.
// The headerless mode declares only the Win32 ABI surface needed by this plugin;
// all functions and messages are documented in Microsoft's Win32 APIs.

#if defined(TOKENSTATUS_HEADERLESS)
using HWND = void*;
using HINSTANCE = void*;
using DWORD_PTR = unsigned long long;
using UINT_PTR = unsigned long long;
using WPARAM = unsigned long long;
using LPARAM = long long;
using LRESULT = long long;
using UINT = unsigned int;
using BOOL = int;
using UCHAR = unsigned char;
using WCHAR = wchar_t;
using LPCWSTR = const wchar_t*;
using DWORD = unsigned long;
#define CALLBACK __stdcall
#define WINAPI __stdcall
extern "C" __declspec(dllimport) LRESULT WINAPI SendMessageW(HWND, UINT, WPARAM, LPARAM);
extern "C" __declspec(dllimport) HWND WINAPI FindWindowExW(HWND, HWND, LPCWSTR, LPCWSTR);
extern "C" __declspec(dllimport) UINT WINAPI GetDpiForWindow(HWND);
extern "C" __declspec(dllimport) int WINAPI MessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT);
using SUBCLASSPROC = LRESULT (CALLBACK*)(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
extern "C" __declspec(dllimport) BOOL WINAPI SetWindowSubclass(HWND, SUBCLASSPROC, UINT_PTR, DWORD_PTR);
extern "C" __declspec(dllimport) BOOL WINAPI RemoveWindowSubclass(HWND, SUBCLASSPROC, UINT_PTR);
extern "C" __declspec(dllimport) LRESULT WINAPI DefSubclassProc(HWND, UINT, WPARAM, LPARAM);
#else
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#endif

#include "token_utils.h"

#if defined(TOKENSTATUS_HEADERLESS) && !defined(TOKENSTATUS_EMULATION)
// Clang may lower zero-initialised local arrays to memset even with no CRT.
extern "C" void* __cdecl memset(void* destination, int value, unsigned long long bytes) {
    volatile unsigned char* p = (volatile unsigned char*)destination;
    for (unsigned long long i = 0; i < bytes; ++i) p[i] = (unsigned char)value;
    return destination;
}
#endif

// ABI-compatible subset of the official Notepad++ PluginInterface.h.
// https://github.com/notepad-plus-plus/notepad-plus-plus/tree/master/PowerEditor/src/MISC/PluginsManager
struct NppData { HWND _nppHandle; HWND _scintillaMainHandle; HWND _scintillaSecondHandle; };
struct ShortcutKey { bool _isCtrl; bool _isAlt; bool _isShift; UCHAR _key; };
struct FuncItem {
    wchar_t _itemName[64];
    void (__cdecl *_pFunc)();
    int _cmdID;
    bool _init2Check;
    ShortcutKey* _pShKey;
};
struct ScintillaNotificationPrefix {
    HWND hwndFrom;
    UINT_PTR idFrom;
    UINT code;
};

// Notepad++ and Scintilla public message constants.
static constexpr UINT NPPMSG = 1024 + 1000;  // WM_USER + 1000
static constexpr UINT NPPM_GETCURRENTSCINTILLA = NPPMSG + 4;
static constexpr UINT NPPM_SETMENUITEMCHECK = NPPMSG + 40;
static constexpr UINT NPPN_READY = 1001;
static constexpr UINT NPPN_SHUTDOWN = 1009;
static constexpr UINT NPPN_BUFFERACTIVATED = 1010;
static constexpr UINT SCI_GETLENGTH = 2006;
static constexpr UINT SB_SETTEXTW_ = 1024 + 11;
static constexpr UINT SB_GETTEXTLENGTHW_ = 1024 + 12;
static constexpr UINT SB_GETTEXTW_ = 1024 + 13;
static constexpr UINT SB_GETPARTS_ = 1024 + 6;
static constexpr UINT SB_SETPARTS_ = 1024 + 4;
static constexpr UINT WM_NCDESTROY_ = 0x0082;
static constexpr UINT STATUSBAR_SIZE_PART = 1;
static constexpr UINT_PTR SUBCLASS_COOKIE = 0x54'4f'4b'53ULL; // TOKS
static constexpr unsigned int TEXT_CAP = 512;
static constexpr unsigned int PART_COUNT = 6;

static NppData g_npp = {};
static FuncItem g_menu[2] = {};
static HWND g_status = nullptr;
static bool g_enabled = true;
static bool g_isHooked = false;
static bool g_ownTextUpdate = false;
static bool g_haveRawParts = false;
static int g_rawParts[PART_COUNT] = {};
static wchar_t g_baseText[TEXT_CAP] = {};
static wchar_t g_renderedText[TEXT_CAP] = {};

static unsigned long long current_document_bytes() {
    if (!g_npp._nppHandle) return 0;
    int active = 0;
    SendMessageW(g_npp._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&active);
    HWND scintilla = (active == 1) ? g_npp._scintillaSecondHandle : g_npp._scintillaMainHandle;
    if (!scintilla) return 0;
    LRESULT len = SendMessageW(scintilla, SCI_GETLENGTH, 0, 0);
    return len > 0 ? (unsigned long long)len : 0ULL;
}

static void compose_current() {
    compose_status(g_baseText, current_document_bytes(), g_renderedText, TEXT_CAP);
}

// Apply a status update without treating our own replacement as a new base string.
static void render_now() {
    if (!g_isHooked || !g_enabled || !g_status) return;
    compose_current();
    g_ownTextUpdate = true;
    SendMessageW(g_status, SB_SETTEXTW_, STATUSBAR_SIZE_PART, (LPARAM)g_renderedText);
    g_ownTextUpdate = false;
}

static int expanded_boundary(int original, HWND bar) {
    if (original <= 0) return original; // Don't make the language panel negative.
    UINT dpi = GetDpiForWindow(bar);
    if (dpi == 0 || dpi > 768) dpi = 96;
    int extra = (int)((135ULL * (unsigned long long)dpi) / 96ULL);
    return original > extra ? original - extra : 0;
}

static void apply_parts() {
    if (!g_isHooked || !g_status || !g_haveRawParts) return;
    // Subclass receives this and adjusts only part 0's right edge.
    SendMessageW(g_status, SB_SETPARTS_, PART_COUNT, (LPARAM)g_rawParts);
}

static void capture_status_base() {
    if (!g_status) return;
    LRESULT size = SendMessageW(g_status, SB_GETTEXTLENGTHW_, STATUSBAR_SIZE_PART, 0);
    unsigned int count = (unsigned int)((unsigned long long)size & 0xFFFFULL);
    if (count >= TEXT_CAP - 1) return;
    wchar_t saved[TEXT_CAP] = {};
    SendMessageW(g_status, SB_GETTEXTW_, STATUSBAR_SIZE_PART, (LPARAM)saved);
    wcopy_bounded(g_baseText, TEXT_CAP, saved);
}

static LRESULT CALLBACK status_subclass(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                        UINT_PTR /*subclassId*/, DWORD_PTR /*context*/) {
    if (msg == SB_SETPARTS_ && (unsigned int)wParam == PART_COUNT && lParam != 0) {
        const int* incoming = (const int*)lParam;
        int widened[PART_COUNT];
        for (unsigned int i = 0; i < PART_COUNT; ++i) {
            g_rawParts[i] = incoming[i];
            widened[i] = incoming[i];
        }
        g_haveRawParts = true;
        if (g_enabled) widened[0] = expanded_boundary(widened[0], hWnd);
        return DefSubclassProc(hWnd, msg, wParam, (LPARAM)widened);
    }
    if (msg == SB_SETTEXTW_ && ((unsigned int)wParam & 0xFFU) == STATUSBAR_SIZE_PART && lParam != 0) {
        if (!g_ownTextUpdate) {
            const wchar_t* nativeText = (const wchar_t*)lParam;
            // A different plugin may write back our own caption: don't append twice.
            if (!wequal(nativeText, g_renderedText))
                wcopy_bounded(g_baseText, TEXT_CAP, nativeText);
            if (g_enabled) {
                compose_current();
                return DefSubclassProc(hWnd, msg, wParam, (LPARAM)g_renderedText);
            }
        }
    }
    if (msg == WM_NCDESTROY_) {
        g_status = nullptr;
        g_isHooked = false;
        g_haveRawParts = false;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

static void attach() {
    if (g_isHooked || !g_npp._nppHandle) return;
    HWND bar = FindWindowExW(g_npp._nppHandle, nullptr, L"msctls_statusbar32", nullptr);
    if (!bar) return;
    g_status = bar;
    if (!SetWindowSubclass(bar, status_subclass, SUBCLASS_COOKIE, 0)) {
        g_status = nullptr;
        return;
    }
    g_isHooked = true;
    capture_status_base();
    int incoming[PART_COUNT] = {};
    if (SendMessageW(bar, SB_GETPARTS_, PART_COUNT, (LPARAM)incoming) == PART_COUNT) {
        for (unsigned int i = 0; i < PART_COUNT; ++i) g_rawParts[i] = incoming[i];
        g_haveRawParts = true;
        apply_parts();
    }
    render_now();
}

static void detach() {
    if (!g_isHooked || !g_status) return;
    g_enabled = false;
    g_ownTextUpdate = true;
    SendMessageW(g_status, SB_SETTEXTW_, STATUSBAR_SIZE_PART, (LPARAM)g_baseText);
    g_ownTextUpdate = false;
    apply_parts(); // Restores host-provided widths.
    RemoveWindowSubclass(g_status, status_subclass, SUBCLASS_COOKIE);
    g_status = nullptr;
    g_isHooked = false;
    g_haveRawParts = false;
}

static void __cdecl toggle() {
    g_enabled = !g_enabled;
    if (g_isHooked && g_status) {
        if (g_enabled) {
            apply_parts();
            render_now();
        } else {
            g_ownTextUpdate = true;
            SendMessageW(g_status, SB_SETTEXTW_, STATUSBAR_SIZE_PART, (LPARAM)g_baseText);
            g_ownTextUpdate = false;
            apply_parts();
        }
    }
    if (g_npp._nppHandle && g_menu[0]._cmdID > 0)
        SendMessageW(g_npp._nppHandle, NPPM_SETMENUITEMCHECK,
                     (WPARAM)g_menu[0]._cmdID, (LPARAM)(g_enabled ? 1 : 0));
}

static void __cdecl about() {
    MessageBoxW(g_npp._nppHandle,
        L"Shows tokens: ~N alongside length and lines in the native status bar.\n\n"
        L"Approximation: document length in bytes / 4, rounded.\n"
        L"Works without network access or tokenizer dependencies.\n"
        L"For source code and non-Latin text, actual model token counts can vary widely.\n\n"
        L"Use Plugins > TokenStatus > Show approximate tokens to switch off.",
        L"TokenStatus 1.0", 0x40 /* MB_ICONINFORMATION */);
}

// Official Notepad++ plugin entrypoints.
extern "C" __declspec(dllexport) void __cdecl setInfo(NppData data) {
    g_npp = data;
}
extern "C" __declspec(dllexport) const wchar_t* __cdecl getName() {
    return L"TokenStatus";
}
extern "C" __declspec(dllexport) FuncItem* __cdecl getFuncsArray(int* count) {
    if (count) *count = 2;
    wcopy_bounded(g_menu[0]._itemName, 64, L"Show approximate tokens");
    g_menu[0]._pFunc = toggle;
    g_menu[0]._init2Check = true;
    wcopy_bounded(g_menu[1]._itemName, 64, L"About TokenStatus...");
    g_menu[1]._pFunc = about;
    return g_menu;
}
extern "C" __declspec(dllexport) void __cdecl beNotified(ScintillaNotificationPrefix* notice) {
    if (!notice) return;
    if (notice->code == NPPN_READY) attach();
    else if (notice->code == NPPN_BUFFERACTIVATED) render_now();
    else if (notice->code == NPPN_SHUTDOWN) detach();
}
extern "C" __declspec(dllexport) LRESULT __cdecl messageProc(UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/) {
    return 1;
}
extern "C" __declspec(dllexport) BOOL __cdecl isUnicode() {
    return 1;
}
