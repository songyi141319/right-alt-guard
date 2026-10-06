#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>

#ifdef _WIN64
static const wchar_t* dllName = L"MenuGuard-v2-x64.dll";
static const wchar_t* objectPrefix = L"Local\\DoubaoRightAltMenuGuard-v1-x64";
#else
static const wchar_t* dllName = L"MenuGuard-v2-x86.dll";
static const wchar_t* objectPrefix = L"Local\\DoubaoRightAltMenuGuard-v1-x86";
#endif

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const std::wstring prefix(objectPrefix);
    const std::wstring stopName = prefix + L"-Stop";
    const std::wstring readyName = prefix + L"-Ready";
    int count = 0;
    wchar_t** args = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!args) return 2;
    bool stopRequested = count == 2 && wcscmp(args[1], L"--stop") == 0;
    bool statusRequested = count == 2 && wcscmp(args[1], L"--status") == 0;
    bool invalidArgs = count > 1 && !stopRequested && !statusRequested;
    LocalFree(args);
    if (invalidArgs) return 2;
    if (stopRequested || statusRequested) {
        HANDLE event = OpenEventW(stopRequested ? EVENT_MODIFY_STATE : SYNCHRONIZE, FALSE,
                                  stopRequested ? stopName.c_str() : readyName.c_str());
        if (!event) return statusRequested ? 1 : 0;
        bool success = stopRequested ? SetEvent(event) != FALSE :
                                      WaitForSingleObject(event, 0) == WAIT_OBJECT_0;
        CloseHandle(event);
        return success ? 0 : 1;
    }

    HANDLE mutex = CreateMutexW(nullptr, FALSE, prefix.c_str());
    if (!mutex) return 3;
    DWORD ownership = WaitForSingleObject(mutex, 0);
    if (ownership != WAIT_OBJECT_0 && ownership != WAIT_ABANDONED) {
        CloseHandle(mutex);
        return ownership == WAIT_TIMEOUT ? 0 : 3;
    }
    HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, stopName.c_str());
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
    if (!stop || !ready) {
        if (stop) CloseHandle(stop);
        if (ready) CloseHandle(ready);
        CloseHandle(mutex);
        return 4;
    }
    ResetEvent(ready);
    wchar_t path[32768];
    DWORD length = GetModuleFileNameW(nullptr, path, 32768);
    std::wstring dllPath(path, length);
    size_t separator = dllPath.find_last_of(L"\\/");
    HMODULE library = nullptr;
    if (length && length < 32768 && separator != std::wstring::npos) {
        dllPath.erase(separator + 1);
        dllPath += dllName;
        library = LoadLibraryExW(dllPath.c_str(), nullptr,
                                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    HOOKPROC messageProc = library ? reinterpret_cast<HOOKPROC>(GetProcAddress(library, "MenuGuardMessages")) : nullptr;
    HOOKPROC commandProc = library ? reinterpret_cast<HOOKPROC>(GetProcAddress(library, "MenuGuardCommands")) : nullptr;
    HHOOK messages = messageProc ? SetWindowsHookExW(WH_GETMESSAGE, messageProc, library, 0) : nullptr;
    HHOOK commands = messages && commandProc ? SetWindowsHookExW(WH_CBT, commandProc, library, 0) : nullptr;
    int result = commands ? 0 : 5;
    if (commands) {
        SetEvent(ready);
        bool running = true;
        while (running) {
            DWORD wake = MsgWaitForMultipleObjects(1, &stop, FALSE, INFINITE, QS_ALLINPUT);
            if (wake == WAIT_OBJECT_0) break;
            if (wake == WAIT_FAILED) { result = 6; break; }
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) { running = false; break; }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    }
    ResetEvent(ready);
    if (commands) UnhookWindowsHookEx(commands);
    if (messages) UnhookWindowsHookEx(messages);
    if (library) FreeLibrary(library);
    CloseHandle(ready);
    CloseHandle(stop);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return result;
}
