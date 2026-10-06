#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "menu_policy.h"

// State stays in the target UI thread. Nothing is shared or saved to disk.
static thread_local MenuPolicy policy;
static thread_local HWND hiddenRightAltRoot = nullptr;

static bool OtherModifiersDown() {
    const int keys[] = {VK_CONTROL, VK_SHIFT, VK_LMENU, VK_LWIN, VK_RWIN};
    for (int key : keys) if (GetKeyState(key) & 0x8000) return true;
    return false;
}

static bool HideChromiumRightAlt(const MSG& message, bool up, bool repeat) {
    HWND root = message.hwnd ? GetAncestor(message.hwnd, GA_ROOT) : nullptr;
    if (up) {
        bool paired = root && hiddenRightAltRoot == root;
        hiddenRightAltRoot = nullptr;
        return paired;
    }
    if (repeat) return root && hiddenRightAltRoot == root;
    wchar_t className[64] = {};
    bool chromium = root && GetClassNameW(root, className, 64) &&
                    lstrcmpW(className, L"Chrome_WidgetWin_1") == 0;
    hiddenRightAltRoot = chromium && !OtherModifiersDown() ? root : nullptr;
    return hiddenRightAltRoot != nullptr;
}

#ifndef _WIN64
#pragma comment(linker, "/EXPORT:MenuGuardMessages=_MenuGuardMessages@12")
#pragma comment(linker, "/EXPORT:MenuGuardCommands=_MenuGuardCommands@12")
#endif

extern "C" __declspec(dllexport) LRESULT CALLBACK MenuGuardMessages(
    int code, WPARAM removed, LPARAM data) {
    if (code == HC_ACTION && removed == PM_REMOVE && data) {
        const MSG& message = *reinterpret_cast<const MSG*>(data);
        bool up = message.message == WM_KEYUP || message.message == WM_SYSKEYUP;
        bool down = message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN;
        if (up || down) {
            uintptr_t bits = static_cast<uintptr_t>(message.lParam);
            bool rightAlt = message.wParam == VK_RMENU ||
                            (message.wParam == VK_MENU && (bits & (1u << 24)));
            HWND root = message.hwnd ? GetAncestor(message.hwnd, GA_ROOT) : nullptr;
            ObserveKey(policy, rightAlt, up, (bits & (1u << 30)) != 0,
                       OtherModifiersDown(), reinterpret_cast<uintptr_t>(root), message.time);
        } else if (message.message == WM_LBUTTONDOWN || message.message == WM_RBUTTONDOWN ||
                   message.message == WM_MBUTTONDOWN || message.message == WM_NCLBUTTONDOWN ||
                   message.message == WM_NCRBUTTONDOWN || message.message == WM_XBUTTONDOWN) {
            policy = {};
        }
    }
    // Earlier callbacks in this same hook chain see the original message first.
    // Physical input and thread modifier state stay unchanged; no keys emitted.
    LRESULT next = CallNextHookEx(nullptr, code, removed, data);
    if (code == HC_ACTION && removed == PM_REMOVE && data) {
        MSG& message = *reinterpret_cast<MSG*>(data);
        bool keyboard = message.message == WM_KEYDOWN || message.message == WM_KEYUP ||
                        message.message == WM_SYSKEYDOWN || message.message == WM_SYSKEYUP;
        uintptr_t bits = static_cast<uintptr_t>(message.lParam);
        bool rightAlt = message.wParam == VK_RMENU ||
                        (message.wParam == VK_MENU && (bits & (1u << 24)));
        bool up = message.message == WM_KEYUP || message.message == WM_SYSKEYUP;
        if (keyboard && rightAlt &&
            HideChromiumRightAlt(message, up, (bits & (1u << 30)) != 0)) {
            // Software-managed menus (e.g. Electron) act on Alt messages,
            // without SC_KEYMENU. Limit this path to Chromium window classes
            // and a matched RAlt pair begun without other held modifiers.
            // Other keys keep their original Alt-context bits. Other window
            // classes keep both Alt notifications and use the native veto.
            message.message = WM_NULL;
            message.wParam = 0;
            message.lParam = 0;
        }
    }
    return next;
}

extern "C" __declspec(dllexport) LRESULT CALLBACK MenuGuardCommands(
    int code, WPARAM command, LPARAM character) {
    if (code == HCBT_SYSCOMMAND &&
        BlockMenu(policy, static_cast<uintptr_t>(command), static_cast<intptr_t>(character),
                  reinterpret_cast<uintptr_t>(GetForegroundWindow()), GetTickCount())) {
        return 1;
    }
    return CallNextHookEx(nullptr, code, command, character);
}
