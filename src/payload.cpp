#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <algorithm>
#include <vector>
#include "payload.h"

class FolderLease {
    std::vector<HANDLE> handles;
    bool Open(const std::wstring& path, bool create) {
        DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            DWORD error = GetLastError();
            if (!create || (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) ||
                !CreateDirectoryW(path.c_str(), nullptr)) return false;
        }
        HANDLE directory = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (directory == INVALID_HANDLE_VALUE) return false;
        BY_HANDLE_FILE_INFORMATION info = {};
        bool good = GetFileInformationByHandle(directory, &info) &&
                    (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
        if (!good) { CloseHandle(directory); return false; }
        handles.push_back(directory); // No share-delete: parents cannot be replaced during extraction.
        return true;
    }
public:
    ~FolderLease() { for (HANDLE directory : handles) CloseHandle(directory); }
    bool Acquire(const std::wstring& path) {
        if (path.size() < 3) return false;
        size_t root = 0;
        if (path[1] == L':' && path[2] == L'\\') root = 3;
        else if (path.substr(0, 2) == L"\\\\" && path[2] != L'?' && path[2] != L'.') {
            size_t server = path.find(L'\\', 2);
            size_t share = server == std::wstring::npos ? server : path.find(L'\\', server + 1);
            if (share != std::wstring::npos) root = share + 1;
        }
        if (!root || !Open(path.substr(0, root), false)) return false;
        size_t end = root;
        while (end < path.size()) {
            end = path.find(L'\\', end);
            if (end == std::wstring::npos) end = path.size();
            if (!Open(path.substr(0, end), true)) return false;
            ++end;
        }
        return true;
    }
};
static bool Matches(const std::wstring& file, const BYTE* bytes, DWORD size) {
    HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    BY_HANDLE_FILE_INFORMATION info = {};
    bool good = GetFileInformationByHandle(handle, &info) &&
                !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) &&
                info.nFileSizeHigh == 0 && info.nFileSizeLow == size;
    BYTE buffer[8192];
    DWORD offset = 0;
    while (good && offset < size) {
        DWORD wanted = (std::min)(size - offset, static_cast<DWORD>(sizeof(buffer)));
        DWORD read = 0;
        good = ReadFile(handle, buffer, wanted, &read, nullptr) && read == wanted &&
               memcmp(buffer, bytes + offset, read) == 0;
        offset += read;
    }
    CloseHandle(handle);
    return good;
}
static bool Unpack(HINSTANCE instance, int id, const std::wstring& file) {
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(id), RT_RCDATA);
    DWORD size = resource ? SizeofResource(instance, resource) : 0;
    HGLOBAL loaded = resource ? LoadResource(instance, resource) : nullptr;
    const BYTE* bytes = loaded ? static_cast<const BYTE*>(LockResource(loaded)) : nullptr;
    if (!size || !bytes) return false;
    if (GetFileAttributesW(file.c_str()) != INVALID_FILE_ATTRIBUTES) return Matches(file, bytes, size);
    std::wstring temporary = file + L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
    HANDLE handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    bool good = WriteFile(handle, bytes, size, &written, nullptr) && written == size && FlushFileBuffers(handle);
    CloseHandle(handle);
    if (good) good = MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!good) DeleteFileW(temporary.c_str()); // Only the new temporary file owned by this invocation.
    return good && Matches(file, bytes, size);
}
bool PreparePayload(HINSTANCE instance, const std::wstring& folder) {
    FolderLease directories;
    if (!directories.Acquire(folder)) return false;
    const wchar_t* names[] = {L"RightAltGuard-v2-x64.exe", L"MenuGuard-v2-x64.dll",
                             L"RightAltGuard-v2-x86.exe", L"MenuGuard-v2-x86.dll"};
    for (int i = 0; i < 4; ++i) {
        if (!Unpack(instance, 101 + i, folder + L"\\" + names[i])) return false;
    }
    return true;
}
std::wstring DefaultPayloadFolder() {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return {};
    std::wstring folder = std::wstring(local) + L"\\RightAltGuard\\0.1.0";
    CoTaskMemFree(local);
    return folder;
}
