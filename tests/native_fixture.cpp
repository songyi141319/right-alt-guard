// Only private MSG values and a hidden fixture window are used. No SendInput,
// keybd_event, mouse events, foreground changes, or messages to user windows.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string>
#include <string.h>

static int seen = 0;
static LRESULT chained = 0;
static LRESULT CALLBACK Probe(int code, WPARAM command, LPARAM character) {
    LRESULT next = CallNextHookEx(nullptr, code, command, character);
    if (code == HCBT_SYSCOMMAND && (command & 0xFFF0) == SC_KEYMENU) {
        ++seen;
        chained = next;
        return 1; // The fixture never enters a menu loop, even without the guard.
    }
    return next;
}

int main(int argc, char** argv) {
    // --global tests the already running two-architecture guard. Without it,
    // install the real command callback on this private fixture thread only.
    bool global = argc == 2 && strcmp(argv[1], "--global") == 0;
#ifdef _WIN64
    const wchar_t* suffix = L"MenuGuard-v2-x64.dll";
#else
    const wchar_t* suffix = L"MenuGuard-v2-x86.dll";
#endif
    HWND foreground = GetForegroundWindow();
    if (!foreground) { puts("UNAVAILABLE: this desktop has no foreground window"); return 2; }
    for (int key : {VK_CONTROL, VK_SHIFT, VK_LMENU, VK_LWIN, VK_RWIN}) {
        if (GetKeyState(key) & 0x8000) { puts("UNAVAILABLE: a modifier is currently held"); return 2; }
    }
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    std::wstring dllPath(path, length);
    dllPath.erase(dllPath.find_last_of(L"\\/") + 1);
    dllPath += suffix;
    HMODULE library = LoadLibraryExW(dllPath.c_str(), nullptr,
                                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) { puts("FAIL: fixture cannot load guard DLL"); return 1; }
    HOOKPROC observe = reinterpret_cast<HOOKPROC>(GetProcAddress(library, "MenuGuardMessages"));
    HOOKPROC veto = reinterpret_cast<HOOKPROC>(GetProcAddress(library, "MenuGuardCommands"));
    HHOOK localGuard = !global && veto ? SetWindowsHookExW(WH_CBT, veto, library, GetCurrentThreadId()) : nullptr;
    HHOOK probe = SetWindowsHookExW(WH_CBT, Probe, nullptr, GetCurrentThreadId());
    HWND fixture = CreateWindowExW(0, L"STATIC", L"Private menu guard fixture", 0,
                                   0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!observe || !veto || (!global && !localGuard) || !probe || !fixture) {
        puts("FAIL: fixture initialization"); return 1;
    }

    MSG message = {};
    message.hwnd = foreground;
    message.message = WM_SYSKEYDOWN;
    message.wParam = VK_MENU;
    message.lParam = static_cast<LPARAM>(0x21380001u);
    message.time = GetTickCount();
    observe(HC_ACTION, PM_REMOVE, reinterpret_cast<LPARAM>(&message));
    message.message = WM_SYSKEYUP;
    message.wParam = VK_MENU;
    message.lParam = static_cast<LPARAM>(0xE1380001u);
    message.time = GetTickCount();
    observe(HC_ACTION, PM_REMOVE, reinterpret_cast<LPARAM>(&message));
    DefWindowProcW(fixture, WM_SYSCOMMAND, SC_KEYMENU, 0);
    bool rightAltBlocked = seen == 1 && chained == 1;

    message.message = WM_SYSKEYDOWN;
    message.wParam = VK_MENU;
    message.lParam = static_cast<LPARAM>(0x20380001u);
    observe(HC_ACTION, PM_REMOVE, reinterpret_cast<LPARAM>(&message));
    message.message = WM_SYSKEYUP;
    message.lParam = static_cast<LPARAM>(0xE0380001u);
    observe(HC_ACTION, PM_REMOVE, reinterpret_cast<LPARAM>(&message));
    DefWindowProcW(fixture, WM_SYSCOMMAND, SC_KEYMENU, 0);
    bool leftAltPassed = seen == 2 && chained == 0;
    bool focusUnchanged = GetForegroundWindow() == foreground;

    DestroyWindow(fixture);
    UnhookWindowsHookEx(probe);
    if (localGuard) UnhookWindowsHookEx(localGuard);
    FreeLibrary(library);
    printf("native CBT: right Alt veto=%d, left Alt pass=%d, user focus unchanged=%d\n",
           rightAltBlocked, leftAltPassed, focusUnchanged);
    return rightAltBlocked && leftAltPassed && focusUnchanged ? 0 : 1;
}
