#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "payload.h"

static constexpr wchar_t trayName[] = L"Local\\RightAltGuard.Tray.v1";
static constexpr wchar_t trayStopName[] = L"Local\\RightAltGuard.Tray.v1-Stop";
static constexpr wchar_t trayReadyName[] = L"Local\\RightAltGuard.Tray.v1-Ready";
static constexpr wchar_t controlName[] = L"Local\\DoubaoRightAltMenuGuard-v1-Control";
static constexpr wchar_t projectUrl[] = L"https://github.com/songyi141319/right-alt-guard";
static HINSTANCE appInstance;
static std::wstring payloadFolder;
static HWND trayWindow;
static HICON activeIcon, pausedIcon;
static UINT taskbarCreated;
static bool exiting = false;
static HANDLE workerJob;
static HANDLE trayReady;
static bool changing = false;
static DWORD startupStage = 0, startupError = 0;
static const GUID trayGuid = {0x34e928b9, 0x37b8, 0x4eed, {0xb9,0x4d,0x9a,0xbf,0x8d,0x35,0x26,0xdd}};

class Control {
    HANDLE mutex = nullptr;
    bool owned = false;
public:
    Control() {
        mutex = CreateMutexW(nullptr, FALSE, controlName);
        DWORD result = mutex ? WaitForSingleObject(mutex, 10000) : WAIT_FAILED;
        owned = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    }
    ~Control() { if (owned) ReleaseMutex(mutex); if (mutex) CloseHandle(mutex); }
    bool Good() const { return owned; }
};
static std::wstring Prefix(const wchar_t* arch) {
    return std::wstring(L"Local\\DoubaoRightAltMenuGuard-v1-") + arch;
}
static bool WaitReleased(const wchar_t* name, DWORD timeout) {
    HANDLE mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, name);
    if (!mutex) return GetLastError() == ERROR_FILE_NOT_FOUND;
    DWORD result = WaitForSingleObject(mutex, timeout);
    bool released = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
    if (released) ReleaseMutex(mutex);
    CloseHandle(mutex);
    return released;
}
static bool Ready(const wchar_t* arch) {
    std::wstring prefix = Prefix(arch);
    if (WaitReleased(prefix.c_str(), 0)) return false;
    HANDLE ready = OpenEventW(SYNCHRONIZE, FALSE, (prefix + L"-Ready").c_str());
    bool result = ready && WaitForSingleObject(ready, 0) == WAIT_OBJECT_0;
    if (ready) CloseHandle(ready);
    return result;
}
static bool Protected() { return Ready(L"x64") && Ready(L"x86"); }
static bool AnyRunning() {
    return !WaitReleased(Prefix(L"x64").c_str(), 0) ||
           !WaitReleased(Prefix(L"x86").c_str(), 0);
}
static bool StopUnlocked() {
    bool good = true;
    for (const wchar_t* arch : {L"x64", L"x86"}) {
        HANDLE stop = OpenEventW(EVENT_MODIFY_STATE, FALSE, (Prefix(arch) + L"-Stop").c_str());
        if (stop) { if (!SetEvent(stop)) good = false; CloseHandle(stop); }
        else if (GetLastError() != ERROR_FILE_NOT_FOUND) good = false;
    }
    for (const wchar_t* arch : {L"x64", L"x86"}) {
        if (!WaitReleased(Prefix(arch).c_str(), 5000)) good = false;
    }
    return good;
}
static bool StopProtection() { Control control; return control.Good() && StopUnlocked(); }
static bool StartProtection() {
    if (changing) return false;
    changing = true;
    struct EndChange { ~EndChange() { changing = false; } } endChange;
    Control control;
    if (!control.Good() || !PreparePayload(appInstance, payloadFolder) || !StopUnlocked()) return false;
    HANDLE children[2] = {};
    const wchar_t* names[] = {L"RightAltGuard-v2-x64.exe", L"RightAltGuard-v2-x86.exe"};
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    if (!attributeSize) return false;
    std::vector<BYTE> attributeStorage(attributeSize);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeStorage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize)) return false;
    struct AttributeCleanup {
        LPPROC_THREAD_ATTRIBUTE_LIST list;
        ~AttributeCleanup() { DeleteProcThreadAttributeList(list); }
    } attributeCleanup{attributes};
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST,
        &workerJob, sizeof(workerJob), nullptr, nullptr)) return false;
    bool good = true;
    for (int i = 0; i < 2; ++i) {
        std::wstring file = payloadFolder + L"\\" + names[i];
        std::wstring command = L"\"" + file + L"\"";
        STARTUPINFOEXW startup = {};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
        startup.StartupInfo.wShowWindow = SW_HIDE;
        startup.lpAttributeList = attributes;
        PROCESS_INFORMATION process = {};
        // Assign the job atomically with creation, including the suspended phase.
        if (!CreateProcessW(file.c_str(), command.data(), nullptr, nullptr, FALSE,
                            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
                            nullptr, payloadFolder.c_str(), &startup.StartupInfo, &process)) { good = false; break; }
        children[i] = process.hProcess;
        good = ResumeThread(process.hThread) != static_cast<DWORD>(-1);
        CloseHandle(process.hThread);
        if (!good) break;
    }
    DWORD started = GetTickCount();
    while (good && !Protected() && static_cast<DWORD>(GetTickCount() - started) < 5000) {
        for (HANDLE child : children) {
            if (child && WaitForSingleObject(child, 0) == WAIT_OBJECT_0) good = false;
        }
        // Service synchronous hook callbacks while waiting for worker readiness.
        MSG pending;
        PeekMessageW(&pending, nullptr, WM_NULL, WM_NULL, PM_NOREMOVE);
        Sleep(20);
    }
    good = good && Protected();
    if (!good) {
        StopUnlocked();
        for (HANDLE child : children) {
            if (child && WaitForSingleObject(child, 1000) == WAIT_TIMEOUT) {
                TerminateProcess(child, 7); // Only children created in this failed invocation.
                WaitForSingleObject(child, 5000);
            }
        }
    }
    for (HANDLE child : children) if (child) CloseHandle(child);
    return good;
}
static void Error() {
    std::wstring text = L"无法完成操作。请确认专用目录可写，或退出旧版后再试。\n"
        L"Could not complete the action. Check the component folder or exit the older version and retry.";
    if (startupStage) text += L"\nStage " + std::to_wstring(startupStage) + L", Windows code " + std::to_wstring(startupError);
    MessageBoxW(trayWindow, text.c_str(),
        L"右 Alt 守护 / RightAlt Guard", MB_OK | MB_ICONERROR);
}
static bool UpdateIcon(DWORD action) {
    NOTIFYICONDATAW icon = {};
    icon.cbSize = sizeof(icon);
    icon.hWnd = trayWindow;
    icon.uID = 1;
    icon.guidItem = trayGuid;
    icon.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_GUID | NIF_SHOWTIP;
    icon.uCallbackMessage = WM_APP + 1;
    bool on = Protected();
    bool partial = !on && AnyRunning();
    icon.hIcon = on ? activeIcon : pausedIcon;
    wcscpy_s(icon.szTip, on ? L"右 Alt 守护 · 已开启 / RightAlt Guard · On" :
        partial ? L"右 Alt 守护 · 状态异常 / RightAlt Guard · Needs attention" :
        L"右 Alt 守护 · 已暂停 / RightAlt Guard · Paused");
    bool good = Shell_NotifyIconW(action, &icon) != FALSE;
    if (good && action == NIM_ADD) { icon.uVersion = NOTIFYICON_VERSION_4; Shell_NotifyIconW(NIM_SETVERSION, &icon); }
    return good;
}
static void Exit() {
    if (StopProtection()) { exiting = true; DestroyWindow(trayWindow); }
    else Error();
}
static void Command(UINT id) {
    if (changing) return;
    if (id == 1001) {
        bool good = AnyRunning() ? StopProtection() : StartProtection();
        UpdateIcon(NIM_MODIFY);
        if (!good) Error();
    } else if (id == 1002) Exit();
    else if (id == 1003) ShellExecuteW(trayWindow, L"open", projectUrl, nullptr, nullptr, SW_SHOWNORMAL);
}
static void Menu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, L"右 Alt 守护 / RightAlt Guard 0.1.0");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 1001, Protected() ? L"暂停保护 / Pause" :
        AnyRunning() ? L"停止异常后台 / Stop remaining workers" : L"恢复保护 / Resume");
    AppendMenuW(menu, MF_STRING, 1003, L"开源主页 / GitHub");
    AppendMenuW(menu, MF_STRING, 1002, L"退出 / Exit");
    POINT point = {};
    GetCursorPos(&point);
    // Foreground ownership is only requested after an explicit tray click,
    // for standard popup dismissal. The keyboard/menu hooks never focus UI.
    SetForegroundWindow(trayWindow);
    UINT id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                            point.x, point.y, 0, trayWindow, nullptr);
    DestroyMenu(menu);
    PostMessageW(trayWindow, WM_NULL, 0, 0);
    if (id) Command(id);
}
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM value, LPARAM data) {
    if (message == taskbarCreated) { UpdateIcon(NIM_ADD); return 0; }
    if (message == WM_APP + 1) {
        UINT event = LOWORD(data);
        if (event == WM_CONTEXTMENU || event == NIN_SELECT || event == NIN_KEYSELECT) Menu();
        return 0;
    }
    if (message == WM_COMMAND) { Command(LOWORD(value)); return 0; }
    if (message == WM_TIMER) {
        if (!changing && !Protected() && AnyRunning()) StopProtection();
        UpdateIcon(NIM_MODIFY);
        return 0;
    }
    if (message == WM_CLOSE) { Exit(); return 0; }
    if (message == WM_QUERYENDSESSION) { StopProtection(); return TRUE; }
    if (message == WM_ENDSESSION && value) { StopProtection(); return 0; }
    if (message == WM_DESTROY) {
        KillTimer(window, 1);
        NOTIFYICONDATAW icon = {};
        icon.cbSize = sizeof(icon); icon.hWnd = window; icon.guidItem = trayGuid; icon.uFlags = NIF_GUID;
        Shell_NotifyIconW(NIM_DELETE, &icon);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, value, data);
}
static int StopFromCommandLine() {
    HANDLE stop = nullptr;
    if (!WaitReleased(trayName, 0)) {
        DWORD started = GetTickCount();
        do {
            stop = OpenEventW(EVENT_MODIFY_STATE, FALSE, trayStopName);
            if (stop || GetLastError() != ERROR_FILE_NOT_FOUND) break;
            Sleep(20);
        } while (static_cast<DWORD>(GetTickCount() - started) < 1000);
        if (!stop) return 1;
    }
    if (stop) {
        bool good = SetEvent(stop) != FALSE;
        CloseHandle(stop);
        if (!good || !WaitReleased(trayName, 10000)) return 1;
    }
    return StopProtection() ? 0 : 1;
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    appInstance = instance;
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 2;
    int result = 2;
    if (argc == 2 && wcscmp(argv[1], L"--status") == 0) {
        bool ready = Protected();
        if (!WaitReleased(trayName, 0)) {
            HANDLE event = OpenEventW(SYNCHRONIZE, FALSE, trayReadyName);
            ready = ready && event && WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
            if (event) CloseHandle(event);
        }
        result = ready ? 0 : 1;
    }
    else if (argc == 2 && wcscmp(argv[1], L"--stop") == 0) result = StopFromCommandLine();
    else if (argc == 3 && wcscmp(argv[1], L"--extract-only") == 0) {
        wchar_t full[32768];
        DWORD length = GetFullPathNameW(argv[2], 32768, full, nullptr);
        result = length && length < 32768 && PreparePayload(instance, full) ? 0 : 3;
    }
    bool launch = argc == 1;
    LocalFree(argv);
    if (!launch) return result;
    HANDLE mutex = CreateMutexW(nullptr, FALSE, trayName);
    DWORD ownership = mutex ? WaitForSingleObject(mutex, 0) : WAIT_FAILED;
    if (ownership != WAIT_OBJECT_0 && ownership != WAIT_ABANDONED) {
        if (mutex) CloseHandle(mutex);
        return ownership == WAIT_TIMEOUT ? 0 : 3;
    }
    HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, trayStopName);
    trayReady = CreateEventW(nullptr, TRUE, FALSE, trayReadyName);
    if (trayReady) ResetEvent(trayReady);
    payloadFolder = DefaultPayloadFolder();
    workerJob = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    bool jobReady = workerJob && SetInformationJobObject(workerJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    activeIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(201), IMAGE_ICON, 32, 32, 0));
    pausedIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(202), IMAGE_ICON, 32, 32, 0));
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSW cls = {};
    cls.lpfnWndProc = WindowProc; cls.hInstance = instance; cls.lpszClassName = L"RightAltGuard.Tray.v1";
    startupStage = 1;
    ATOM registered = RegisterClassW(&cls);
    if (!registered) startupError = GetLastError();
    else {
        startupStage = 2;
        trayWindow = CreateWindowExW(0, cls.lpszClassName,
            L"右 Alt 守护 / RightAlt Guard", 0, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
        if (!trayWindow) startupError = GetLastError();
    }
    if (trayWindow) startupStage = 3;
    bool started = stop && trayReady && jobReady && trayWindow && activeIcon && pausedIcon &&
                   !payloadFolder.empty() && StartProtection();
    result = started ? 0 : 4;
    if (started) startupStage = 4;
    if (started) {
        if (!UpdateIcon(NIM_ADD)) result = 5;
        else {
            startupStage = 0;
            SetEvent(trayReady);
            SetTimer(trayWindow, 1, 2000, nullptr);
            while (!exiting) {
                DWORD wake = MsgWaitForMultipleObjects(1, &stop, FALSE, INFINITE, QS_ALLINPUT);
                if (wake == WAIT_OBJECT_0) { Exit(); if (!exiting) { result = 6; break; } }
                else if (wake == WAIT_FAILED) { result = 6; break; }
                MSG message;
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                    if (message.message == WM_QUIT) { exiting = true; break; }
                    TranslateMessage(&message); DispatchMessageW(&message);
                }
            }
        }
        if (!StopProtection()) result = 6;
    }
    if (result) Error();
    if (trayWindow && IsWindow(trayWindow)) DestroyWindow(trayWindow);
    if (activeIcon) DestroyIcon(activeIcon);
    if (pausedIcon) DestroyIcon(pausedIcon);
    if (workerJob) CloseHandle(workerJob);
    if (trayReady) { ResetEvent(trayReady); CloseHandle(trayReady); }
    if (stop) CloseHandle(stop);
    ReleaseMutex(mutex); CloseHandle(mutex);
    return result;
}
