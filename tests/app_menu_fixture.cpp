// Private queued messages only. No physical input or user-window interaction.
// Model the public Electron RootView Alt-down/Alt-up menu focus sequence.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>

static int checks = 0;
static int failures = 0;
static int originalAltMessages = 0;
static bool altPending = false;
static bool menuFocused = false;
static bool acceleratorVisible = false;
static bool shortcutActivated = false;

static void Expect(const char* name, bool value) {
    ++checks;
    if (!value) { ++failures; printf("FAIL: %s\n", name); }
}
static bool IsAlt(const MSG& m) {
    return m.wParam == VK_MENU || m.wParam == VK_LMENU || m.wParam == VK_RMENU;
}
static LRESULT CALLBACK ExistingObserver(int code, WPARAM removed, LPARAM data) {
    if (code == HC_ACTION && removed == PM_REMOVE && data) {
        const MSG& m = *reinterpret_cast<const MSG*>(data);
        if ((m.message == WM_SYSKEYDOWN || m.message == WM_SYSKEYUP) && IsAlt(m)) ++originalAltMessages;
    }
    return CallNextHookEx(nullptr, code, removed, data);
}
static void AppReceives(const MSG& m) {
    if (m.message == WM_NULL) return;
    if ((m.message == WM_SYSKEYDOWN || m.message == WM_KEYDOWN) && IsAlt(m)) {
        acceleratorVisible = true;
        altPending = true;
    } else if ((m.message == WM_SYSKEYUP || m.message == WM_KEYUP) && IsAlt(m)) {
        acceleratorVisible = false;
        if (altPending) { menuFocused = true; acceleratorVisible = true; }
        altPending = false;
    } else {
        if (m.message == WM_SYSKEYDOWN && m.wParam == 'F' &&
            (static_cast<uintptr_t>(m.lParam) & (1u << 29))) shortcutActivated = true;
        altPending = false;
    }
}
static MSG Take(HWND window, UINT message, WPARAM key, LPARAM bits) {
    MSG m = {};
    if (!PostMessageW(window, message, key, bits) || !PeekMessageW(&m, window, 0, 0, PM_REMOVE)) {
        puts("FAIL: private message queue"); ++failures;
    }
    return m;
}

int main(int argc, char** argv) {
    bool global = argc == 2 && strcmp(argv[1], "--global") == 0;
    if (argc != 1 && !global) return 2;
#ifdef _WIN64
    const wchar_t* name = L"MenuGuard-v2-x64.dll";
#else
    const wchar_t* name = L"MenuGuard-v2-x86.dll";
#endif
    BYTE saved[256];
    if (!GetKeyboardState(saved)) return 2;
    BYTE keys[256] = {};
    SetKeyboardState(keys); // Only this private fixture thread; no key events.
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    std::wstring libraryPath(path, length);
    libraryPath.erase(libraryPath.find_last_of(L"\\/") + 1);
    libraryPath += name;
    HMODULE library = global ? nullptr : LoadLibraryExW(libraryPath.c_str(), nullptr,
                                    LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    HOOKPROC observe = library ? reinterpret_cast<HOOKPROC>(GetProcAddress(library, "MenuGuardMessages")) : nullptr;
    WNDCLASSW privateClass = {};
    privateClass.lpfnWndProc = DefWindowProcW;
    privateClass.hInstance = GetModuleHandleW(nullptr);
    privateClass.lpszClassName = L"Chrome_WidgetWin_1";
    if (!RegisterClassW(&privateClass)) { SetKeyboardState(saved); return 2; }
    HWND window = CreateWindowExW(0, privateClass.lpszClassName, L"Private app-menu fixture", 0,
                                  0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HWND ordinary = CreateWindowExW(0, L"STATIC", L"Private ordinary fixture", 0,
                                    0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    HHOOK existing = SetWindowsHookExW(WH_GETMESSAGE, ExistingObserver, nullptr, GetCurrentThreadId());
    HHOOK guard = observe ? SetWindowsHookExW(WH_GETMESSAGE, observe, library, GetCurrentThreadId()) : nullptr;
    if ((!global && (!library || !guard)) || !window || !ordinary || !existing) { SetKeyboardState(saved); return 2; }
    MSG clear;
    while (PeekMessageW(&clear, window, 0, 0, PM_REMOVE)) {}
    while (PeekMessageW(&clear, ordinary, 0, 0, PM_REMOVE)) {}

    PostMessageW(window, WM_SYSKEYDOWN, VK_MENU, static_cast<LPARAM>(0x21380001u));
    MSG preview = {};
    PeekMessageW(&preview, window, 0, 0, PM_NOREMOVE);
    Expect("non-removing preview remains an original Alt message", preview.message == WM_SYSKEYDOWN && preview.wParam == VK_MENU);
    MSG rightDown = {};
    PeekMessageW(&rightDown, window, 0, 0, PM_REMOVE);
    AppReceives(rightDown);
    MSG rightUp = Take(window, WM_SYSKEYUP, VK_MENU, static_cast<LPARAM>(0xE1380001u));
    AppReceives(rightUp);
    Expect("bare right Alt does not focus a software-managed menu", !menuFocused);
    Expect("bare right Alt does not leave accelerators highlighted", !acceleratorVisible);
    Expect("existing input observers receive both original Alt events", originalAltMessages == 2);

    altPending = menuFocused = acceleratorVisible = false;
    AppReceives(Take(window, WM_SYSKEYDOWN, VK_MENU, static_cast<LPARAM>(0x20380001u)));
    AppReceives(Take(window, WM_SYSKEYUP, VK_MENU, static_cast<LPARAM>(0xE0380001u)));
    Expect("left Alt keeps normal menu focus behavior", menuFocused);

    keys[VK_MENU] = keys[VK_RMENU] = 0x80;
    SetKeyboardState(keys);
    Take(window, WM_SYSKEYDOWN, VK_MENU, static_cast<LPARAM>(0x21380001u));
    Expect("filter does not clear the private thread Alt modifier state", (GetKeyState(VK_RMENU) & 0x8000) != 0);
    MSG letter = Take(window, WM_SYSKEYDOWN, 'F', static_cast<LPARAM>(0x20210001u));
    Expect("Alt F keeps its original key and Alt-context flag", letter.message == WM_SYSKEYDOWN && letter.wParam == 'F' && letter.lParam == static_cast<LPARAM>(0x20210001u));
    shortcutActivated = false;
    AppReceives(letter);
    Expect("software-menu model still handles Alt F accelerator", shortcutActivated);
    MSG tab = Take(window, WM_SYSKEYDOWN, VK_TAB, static_cast<LPARAM>(0x200F0001u));
    Expect("Alt Tab keeps its original key and Alt-context flag", tab.message == WM_SYSKEYDOWN && tab.wParam == VK_TAB && tab.lParam == static_cast<LPARAM>(0x200F0001u));
    keys[VK_MENU] = keys[VK_RMENU] = 0;
    SetKeyboardState(keys);
    Take(window, WM_SYSKEYUP, VK_MENU, static_cast<LPARAM>(0xE1380001u));
    Expect("filter does not leave Alt logically held after release", (GetKeyState(VK_RMENU) & 0x8000) == 0);
    MSG character = Take(window, WM_CHAR, 'x', 1);
    Expect("ordinary input characters remain unchanged", character.message == WM_CHAR && character.wParam == 'x' && character.lParam == 1);

    altPending = menuFocused = acceleratorVisible = false;
    AppReceives(Take(window, WM_KEYDOWN, VK_RMENU, static_cast<LPARAM>(0x01380001u)));
    AppReceives(Take(window, WM_SYSKEYUP, VK_RMENU, static_cast<LPARAM>(0xE1380001u)));
    Expect("explicit right Alt virtual-key messages cannot focus menu", !menuFocused && !acceleratorVisible);

    MSG ordinaryDown = Take(ordinary, WM_SYSKEYDOWN, VK_MENU, static_cast<LPARAM>(0x21380001u));
    // Model a message-driven application's Alt+F4 state tracking (e.g. SDL2).
    bool ordinaryAltHeld = ordinaryDown.message == WM_SYSKEYDOWN && IsAlt(ordinaryDown);
    MSG closeKey = Take(ordinary, WM_SYSKEYDOWN, VK_F4, static_cast<LPARAM>(0x203E0001u));
    Expect("message-driven Alt F4 still has its prior Alt-down notification", ordinaryAltHeld && closeKey.message == WM_SYSKEYDOWN && closeKey.wParam == VK_F4);
    MSG ordinaryUp = Take(ordinary, WM_SYSKEYUP, VK_MENU, static_cast<LPARAM>(0xE1380001u));
    Expect("other window classes keep both right Alt messages", ordinaryDown.message == WM_SYSKEYDOWN && ordinaryDown.wParam == VK_MENU && ordinaryUp.message == WM_SYSKEYUP && ordinaryUp.wParam == VK_MENU);

    const int modifiers[] = {VK_CONTROL, VK_SHIFT, VK_LMENU, VK_LWIN, VK_RWIN};
    for (int modifier : modifiers) {
        keys[modifier] = 0x80;
        SetKeyboardState(keys);
        MSG combinedDown = Take(window, WM_SYSKEYDOWN, VK_MENU, static_cast<LPARAM>(0x21380001u));
        keys[modifier] = 0;
        SetKeyboardState(keys);
        MSG combinedUp = Take(window, WM_SYSKEYUP, VK_MENU, static_cast<LPARAM>(0xE1380001u));
        Expect("right Alt with a pre-held modifier retains its down/up pair", combinedDown.message == WM_SYSKEYDOWN && combinedUp.message == WM_SYSKEYUP);
    }
    MSG unmatchedUp = Take(window, WM_SYSKEYUP, VK_RMENU, static_cast<LPARAM>(0xE1380001u));
    Expect("an unmatched right Alt release is passed through", unmatchedUp.message == WM_SYSKEYUP && unmatchedUp.wParam == VK_RMENU);
    MSG pairedDown = Take(window, WM_SYSKEYDOWN, VK_MENU, static_cast<LPARAM>(0x21380001u));
    MSG repeatDown = Take(window, WM_SYSKEYDOWN, VK_MENU, static_cast<LPARAM>(0x61380001u));
    keys[VK_SHIFT] = 0x80;
    SetKeyboardState(keys);
    MSG pairedUp = Take(window, WM_SYSKEYUP, VK_MENU, static_cast<LPARAM>(0xE1380001u));
    Expect("a hidden down keeps its repeat and up paired when another modifier arrives", pairedDown.message == WM_NULL && repeatDown.message == WM_NULL && pairedUp.message == WM_NULL);
    keys[VK_SHIFT] = 0;
    SetKeyboardState(keys);
    Take(window, WM_SYSKEYDOWN, VK_MENU, static_cast<LPARAM>(0x21380001u));
    MSG changedWindowUp = Take(ordinary, WM_SYSKEYUP, VK_MENU, static_cast<LPARAM>(0xE1380001u));
    Expect("a release delivered to a different window is passed through", changedWindowUp.message == WM_SYSKEYUP);

    if (guard) UnhookWindowsHookEx(guard);
    UnhookWindowsHookEx(existing);
    DestroyWindow(window);
    DestroyWindow(ordinary);
    UnregisterClassW(privateClass.lpszClassName, privateClass.hInstance);
    if (library) FreeLibrary(library);
    SetKeyboardState(saved);
    printf("%s: %d checks, %d failures\n", global ? "global guard" : "local guard", checks, failures);
    return failures ? 1 : 0;
}
