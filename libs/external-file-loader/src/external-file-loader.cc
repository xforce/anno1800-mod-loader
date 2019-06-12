#include "external-file-loader.h"
#include "mod.h"
#include "mod_manager.h"

#include "anno/file.h"
#include "anno/random_game_functions.h"
#include "hooking.h"

#include "spdlog/spdlog.h"

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

class Mod;

std::vector<Mod> mods;

uintptr_t* ReadFileFromContainerOIP = nullptr;
bool __fastcall ReadFileFromContainer(__int64 archive_file_map, const std::wstring& file_path,
                                      char** output_data_pointer, size_t* output_data_size)
{
    auto m = file_path;
    // archive_file_map is a pointer to a struct identifying which rda this file resides in
    // as each rda is actually just a memory mapped file
    // but we don't care about that at the moment, probably never will
    if (ModManager::instance().IsFileModded(m)) {
        auto info = ModManager::instance().GetModdedFileInfo(m);
        if (info.is_patched) {
            memcpy(*output_data_pointer, info.data.data(), info.data.size());
        } else {
            // This is not a file that we can patch
            // Just load it from disk
            auto hFile = CreateFileW(info.disk_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                     OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                return false;
            }
            LARGE_INTEGER lFileSize;
            GetFileSizeEx(hFile, &lFileSize);
            DWORD read = 0;
            ReadFile(hFile, *output_data_pointer, *output_data_size, &read, NULL);
            CloseHandle(hFile);
        }
        return true;
    }
    auto result = anno::ReadFileFromContainer(archive_file_map, file_path, output_data_pointer,
                                              output_data_size);
    result      = result;
    return result;
}

bool GetContainerBlockInfo(anno::rdsdk::CFile* file, const std::wstring& file_path, int a3)
{
    if (file_path.find(L"checksum.db") != std::wstring::npos) {
        ModManager::instance().GameFilesReady();
    }
    if (!fs::exists(ModManager::GetModsDirectory() / "dummy")) {
        std::fstream fs;
        fs.open(ModManager::GetModsDirectory() / "dummy", std::ios::out);
        fs.close();
    }
    auto       m         = file_path;
    const auto game_size = anno::rdsdk::CFile::GetFileSize(file_path);
    if (game_size == 0) {
        // File does not exist in RDA
        if ((ModManager::instance().IsFileModded(file_path)
             || m.find(L"data/config/game/asset") == 0)
            && m.find(L"data/shaders/cache") != 0) {
            m = L"mods/dummy";
        }
    }
    auto result = anno::GetContainerBlockInfo((uintptr_t*)file, m, a3);
    if (m == L"mods/dummy") {
        file->file_path = file_path;
    }
    if (ModManager::instance().IsFileModded(file_path)) {
        auto info  = ModManager::instance().GetModdedFileInfo(file_path);
        file->size = info.size;
    } else {
        m = ModManager::MapAliasedPath(file_path);
        if (file->size == 0) {
            file->size = anno::rdsdk::CFile::GetFileSize(m);
        }
    }

    return result;
}
inline size_t GetFileSize(fs::path m)
{
    size_t size = 0;
    if (ModManager::instance().IsFileModded(m)) {
        const auto& info = ModManager::instance().GetModdedFileInfo(m);
        if (info.is_patched) {
            size = info.data.size();
        } else {
            // This is not a file that we can patch
            // Just load it from disk
            auto hFile = CreateFileW(info.disk_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                     OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                return size;
            }
            LARGE_INTEGER lFileSize;
            GetFileSizeEx(hFile, &lFileSize);
            CloseHandle(hFile);

            size = lFileSize.QuadPart;
        }
    } else {
        size = anno::rdsdk::CFile::GetFileSize(m);
    }
    return size;
}

HANDLE FindFirstFileW_S(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
    auto n = FindFirstFileW(lpFileName, lpFindFileData);
    //
    auto w_str    = std::wstring(lpFileName);
    auto mod_path = ModManager::MapAliasedPath(w_str);
    if (ModManager::instance().IsFileModded(mod_path)
        || w_str.find(L"data/config/game/asset") == 0) {
        //
        if (n != INVALID_HANDLE_VALUE) {
            FindClose(n);
        }
        size_t         size = GetFileSize(mod_path);
        ULARGE_INTEGER nsize;
        nsize.QuadPart = size;

        auto p = ModManager::GetModsDirectory();
        n      = FindFirstFileW((p / L"dummy").wstring().c_str(), lpFindFileData);
        lpFindFileData->nFileSizeHigh = nsize.HighPart;
        lpFindFileData->nFileSizeLow  = nsize.LowPart;
        SYSTEMTIME st;
        GetSystemTime(&st);
        SystemTimeToFileTime(&st, &lpFindFileData->ftCreationTime);
        SystemTimeToFileTime(&st, &lpFindFileData->ftLastAccessTime);
        SystemTimeToFileTime(&st, &lpFindFileData->ftLastWriteTime);
    }
    return n;
}

struct Cookie {
    uintptr_t*   vtable_maybe;
    char         pad[0x8];
    std::wstring path;
};

void FileReadAllocateBuffer(Cookie* a1, size_t size)
{
    if (size == 0) {
        size = GetFileSize(ModManager::MapAliasedPath(a1->path));
    }
    func_call<void>(GetAddress(anno::FILE_READ_ALLOCATE_BUFFER), a1, size);
}

uint64_t ReadGameFile(anno::rdsdk::CFile* file, LPVOID lpBuffer, DWORD nNumberOfBytesToRead)
{
    auto m         = ModManager::MapAliasedPath(file->file_path);
    auto file_path = file->file_path;
    if (ModManager::instance().IsFileModded(m)) {
        const auto& info = ModManager::instance().GetModdedFileInfo(m);
        if (info.is_patched) {
            memcpy(lpBuffer, info.data.data(), info.data.size());
            return info.data.size();
        } else {
            // This is not a file that we can patch
            // Just load it from disk
            auto hFile = CreateFileW(info.disk_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                     OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                return 0;
            }

            auto    current_offset                  = file->offset;
            int64_t bytes_left_in_buffer_read_count = 0;

            if (file->size != current_offset) {
                bytes_left_in_buffer_read_count = file->size - current_offset;
            }
            if (nNumberOfBytesToRead < bytes_left_in_buffer_read_count) {
                bytes_left_in_buffer_read_count = nNumberOfBytesToRead;
            }
            if (bytes_left_in_buffer_read_count) {
                SetFilePointer(hFile, current_offset, NULL, FILE_BEGIN);
                DWORD         read = 0;
                LARGE_INTEGER lFileSize;
                GetFileSizeEx(hFile, &lFileSize);
                ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, &read, NULL);
                CloseHandle(hFile);
                file->offset += read;
                return read;
            }
            return bytes_left_in_buffer_read_count;
        }
    } else {
        auto size = anno::rdsdk::CFile::GetFileSize(m);
        if (size > 0 && file->file_handle) {
            size_t output_data_size = 0;
            anno::ReadFileFromContainer(
                *(uintptr_t*)GetAddress(anno::SOME_GLOBAL_STRUCTURE_ARCHIVE), m.c_str(),
                (char**)&lpBuffer, &output_data_size);
            return output_data_size;
        }
        return func_call<uint64_t>(GetAddress(anno::READ_GAME_FILE), file, lpBuffer,
                                   nNumberOfBytesToRead);
    }
}

void EnableExtenalFileLoading(Events& events)
{
    ModManager::instance().LoadMods();

    events.GetProcAddress.connect([](std::string proc_name) {
        if (proc_name == "FindFirstFileW") {
            return (uintptr_t)FindFirstFileW_S;
        }
        return uintptr_t(0);
    });
    set_import("FindFirstFileW", (uintptr_t)FindFirstFileW_S);

    events.DoHooking.connect([]() {
        SetAddress(anno::READ_FILE_FROM_CONTAINER,
                   uintptr_t(detour_func(GetAddress(anno::READ_FILE_FROM_CONTAINER),
                                         ReadFileFromContainer)));
        SetAddress(anno::GET_CONTAINER_BLOCK_INFO,
                   uintptr_t(detour_func(GetAddress(anno::GET_CONTAINER_BLOCK_INFO),
                                         GetContainerBlockInfo)));
        detour_func(GetAddress(anno::READ_GAME_FILE_JMP), ReadGameFile);
        detour_func(GetAddress(anno::FILE_READ_ALLOCATE_BUFFER_JMP), FileReadAllocateBuffer);

        // *(uint32_t*)(0x1458D4E16) = 0x0;

        // retn(0x148423610); // Disables UI rendering...
    });
}
