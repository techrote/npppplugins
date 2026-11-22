// Compiles the actual plugin code against a tiny, deterministic Win32 status-bar
// simulator on Linux. No real Notepad++ or Windows messages are generated.
#define TOKENSTATUS_HEADERLESS
#define TOKENSTATUS_EMULATION
#define __declspec(x)
#define __cdecl
#define __stdcall
#include "../src/TokenStatus.cpp"
#include <cassert>
#include <cwchar>

static int main_hwnd = 1, bar_hwnd = 2, sci1_hwnd = 3, sci2_hwnd = 4;
static SUBCLASSPROC subclass = nullptr;
static int native_parts[6] = {220, 440, 700, 810, 930, 975};
static wchar_t native_caption[512] = L"length: 40    lines: 2";
static long long edit_bytes[2] = {40, 400};
static int which_view = 0;
static bool checked = true;

extern "C" HWND __stdcall FindWindowExW(HWND parent, HWND, LPCWSTR cls, LPCWSTR) {
    if (parent == (HWND)&main_hwnd && std::wcscmp(cls, L"msctls_statusbar32") == 0)
        return (HWND)&bar_hwnd;
    return nullptr;
}
extern "C" UINT __stdcall GetDpiForWindow(HWND) { return 96; }
extern "C" int __stdcall MessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT) { return 1; }
extern "C" BOOL __stdcall SetWindowSubclass(HWND, SUBCLASSPROC proc, UINT_PTR, DWORD_PTR) {
    assert(!subclass); subclass = proc; return 1;
}
extern "C" BOOL __stdcall RemoveWindowSubclass(HWND, SUBCLASSPROC proc, UINT_PTR) {
    assert(subclass == proc); subclass = nullptr; return 1;
}
extern "C" LRESULT __stdcall DefSubclassProc(HWND, UINT msg, WPARAM w, LPARAM l) {
    (void)w;
    if (msg == SB_SETTEXTW_) {
        wcopy_bounded(native_caption, 512, (const wchar_t*)l);
        return 1;
    }
    if (msg == SB_SETPARTS_) {
        for (unsigned i = 0; i < 6; ++i) native_parts[i] = ((const int*)l)[i];
        return 1;
    }
    return 0;
}
extern "C" LRESULT __stdcall SendMessageW(HWND hwnd, UINT msg, WPARAM w, LPARAM l) {
    if (hwnd == (HWND)&main_hwnd && msg == NPPM_GETCURRENTSCINTILLA) {
        *(int*)l = which_view;
        return 1;
    }
    if (hwnd == (HWND)&main_hwnd && msg == NPPM_SETMENUITEMCHECK) {
        checked = l != 0;
        return 1;
    }
    if (hwnd == (HWND)&sci1_hwnd && msg == SCI_GETLENGTH) return edit_bytes[0];
    if (hwnd == (HWND)&sci2_hwnd && msg == SCI_GETLENGTH) return edit_bytes[1];
    if (hwnd != (HWND)&bar_hwnd) return 0;
    if (msg == SB_GETPARTS_) {
        if (l) for (unsigned i = 0; i < 6; ++i) ((int*)l)[i] = native_parts[i];
        return 6;
    }
    if (msg == SB_GETTEXTLENGTHW_) return std::wcslen(native_caption);
    if (msg == SB_GETTEXTW_) {
        wcopy_bounded((wchar_t*)l, 512, native_caption);
        return std::wcslen(native_caption);
    }
    if (subclass && (msg == SB_SETTEXTW_ || msg == SB_SETPARTS_))
        return subclass(hwnd, msg, w, l, SUBCLASS_COOKIE, 0);
    return DefSubclassProc(hwnd, msg, w, l);
}

int main() {
    NppData handles = {(HWND)&main_hwnd, (HWND)&sci1_hwnd, (HWND)&sci2_hwnd};
    setInfo(handles);
    int count = 0;
    FuncItem* menu = getFuncsArray(&count);
    assert(count == 2 && menu[0]._init2Check);
    assert(std::wcscmp(getName(), L"TokenStatus") == 0);
    ScintillaNotificationPrefix ready = {nullptr, 0, NPPN_READY};
    beNotified(&ready);
    assert(subclass != nullptr);
    assert(native_parts[0] == 85); // 220 -> 85: doc-size area widens by 135 px
    assert(std::wcscmp(native_caption, L"length: 40    lines: 2    tokens: ~10") == 0);

    edit_bytes[0] = 8000;
    SendMessageW((HWND)&bar_hwnd, SB_SETTEXTW_, 1, (LPARAM)L"length: 8000    lines: 70");
    assert(std::wcscmp(native_caption, L"length: 8000    lines: 70    tokens: ~2,000") == 0);
    // An external plugin writing our caption back must not duplicate the suffix.
    wchar_t saved[512]; wcopy_bounded(saved, 512, native_caption);
    SendMessageW((HWND)&bar_hwnd, SB_SETTEXTW_, 1, (LPARAM)saved);
    assert(std::wcscmp(native_caption, saved) == 0);

    int resized[6] = {310, 530, 790, 900, 1020, 1065};
    SendMessageW((HWND)&bar_hwnd, SB_SETPARTS_, 6, (LPARAM)resized);
    assert(native_parts[0] == 175 && native_parts[1] == 530);

    menu[0]._cmdID = 42;
    menu[0]._pFunc(); // off
    assert(!checked && native_parts[0] == 310);
    assert(std::wcscmp(native_caption, L"length: 8000    lines: 70") == 0);
    edit_bytes[0] = 104;
    SendMessageW((HWND)&bar_hwnd, SB_SETTEXTW_, 1, (LPARAM)L"length: 104    lines: 4");
    assert(std::wcscmp(native_caption, L"length: 104    lines: 4") == 0);
    menu[0]._pFunc(); // on
    assert(checked && native_parts[0] == 175);
    assert(std::wcscmp(native_caption, L"length: 104    lines: 4    tokens: ~26") == 0);

    // Tab switching to secondary view updates estimate without scanning bytes.
    which_view = 1;
    ScintillaNotificationPrefix activated = {nullptr, 0, NPPN_BUFFERACTIVATED};
    beNotified(&activated);
    assert(std::wcscmp(native_caption, L"length: 104    lines: 4    tokens: ~100") == 0);
    SendMessageW((HWND)&bar_hwnd, SB_SETTEXTW_, 1, (LPARAM)L"length: 400    lines: 4");
    assert(std::wcscmp(native_caption, L"length: 400    lines: 4    tokens: ~100") == 0);

    ScintillaNotificationPrefix shutdown = {nullptr, 0, NPPN_SHUTDOWN};
    beNotified(&shutdown);
    assert(subclass == nullptr);
    assert(native_parts[0] == 310);
    assert(std::wcscmp(native_caption, L"length: 400    lines: 4") == 0);
    return 0;
}
