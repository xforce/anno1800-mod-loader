#include "external-file-loader.h"
#include "mod.h"
#include "mod_manager.h"

#include "anno/random_game_functions.h"
#include "anno/rdgs/regrow_manager.h"
#include "anno/rdsdk/file.h"

#include "meow_hook/detour.h"
// #include "meow_hook/memory.h"
#include "spdlog/spdlog.h"

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>

namespace fs = std::filesystem;

class Mod;

std::vector<Mod> mods;

// #define ADVANCED_HOOK_LOGS 1

uintptr_t* ReadFileFromContainerOIP = nullptr;
bool       ReadFileFromContainer(__int64 archive_file_map, const std::wstring& file_path,
                                 char** output_data_pointer, size_t* output_data_size)
{
    // archive_file_map is a pointer to a struct identifying which rda this file resides in
    // as each rda is actually just a memory mapped file
    // but we don't care about that at the moment, probably never will
    auto mapped_path = ModManager::MapAliasedPath(file_path);
    if (ModManager::instance().IsFileModded(mapped_path)) {
#if defined(ADVANCED_HOOK_LOGS)
        spdlog::debug(L"Read Modded File From Container {} {}", mapped_path.wstring(),
                      *output_data_size);
#endif
        auto info = ModManager::instance().GetModdedFileInfo(mapped_path);
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
    return anno::ReadFileFromContainer(archive_file_map, mapped_path, output_data_pointer,
                                              output_data_size);
}

inline size_t GetFileSize(fs::path m)
{
#if defined(ADVANCED_HOOK_LOGS)
    spdlog::debug("Custom GetFileSize {}", m.string());
#endif
    size_t size = 0;
    auto mapped_path = ModManager::MapAliasedPath(m);
    if (ModManager::instance().IsFileModded(mapped_path)) {
        const auto& info = ModManager::instance().GetModdedFileInfo(mapped_path);
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
        size = anno::rdsdk::CFile::GetFileSize(mapped_path);
    }
#if defined(ADVANCED_HOOK_LOGS)
    spdlog::debug("Custom GetFileSize Result {}", size);
#endif
    return size;
}

inline bool FileGetSize(uintptr_t a1, std::wstring& file_path, size_t* output_size)
{
#if defined(ADVANCED_HOOK_LOGS)
    spdlog::debug(L"FileGetSize {}", file_path);
#endif
    // This is one of the first files that get loaded
    // We absuse this to detect the best place to start out patching process
    // Doing it this way makes sure that all .rda files are loaded so that we can request any file
    // that is used later on in-game
    if (file_path == L"maindata/checksum.db"
        || file_path == L"data/config/export/main/asset/properties.xml"
        || file_path == L"data/graphics/ui/appicons/anno7.png"
        || file_path == L"data/config/engine/enginesettings/default.xml") {
        ModManager::instance().GameFilesReady();
    }
    auto mapped_path = ModManager::MapAliasedPath(file_path);
    if (ModManager::instance().IsFileModded(mapped_path)) {
        const auto& info = ModManager::instance().GetModdedFileInfo(mapped_path);
        if (info.is_patched) {
            *output_size = info.data.size();
        } else {
            // This is not a file that we can patch
            // Just load it from disk
            auto hFile = CreateFileW(info.disk_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                     OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                *output_size = 0;
                return false;
            }
            LARGE_INTEGER lFileSize;
            GetFileSizeEx(hFile, &lFileSize);
            CloseHandle(hFile);

            *output_size = lFileSize.QuadPart;
        }
#if defined(ADVANCED_HOOK_LOGS)
        spdlog::debug(L"rdsdk::CFile::GetFileSize Patched({}) Size({})", info.is_patched,
                      *output_size);
#endif
    } else {
        auto r = anno::rdsdk::CFile::GetFileSize(a1, mapped_path.wstring(), output_size);
#if defined(ADVANCED_HOOK_LOGS)
        spdlog::debug(L"rdsdk::CFile::GetFileSize {} {}", mapped_path.wstring(), *output_size);
#endif
        return r;
    }
    return true;
}

HANDLE FindFirstFileW_S(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
    auto n = FindFirstFileW(lpFileName, lpFindFileData);
    //
    auto w_str = std::wstring(lpFileName);
    if (!w_str.empty()) {
#if defined(ADVANCED_HOOK_LOGS)
        spdlog::debug(L"FindFirstFileW_S {}", w_str);
#endif
    }
    auto mapped_path = ModManager::MapAliasedPath(w_str);
    if (ModManager::instance().IsFileModded(mapped_path)
        || w_str.find(L"data/config/game/asset") == 0) {
        //
        if (n != INVALID_HANDLE_VALUE) {
            FindClose(n);
        }
        size_t         size = GetFileSize(mapped_path);
        ULARGE_INTEGER nsize;
        nsize.QuadPart = size;

        const auto dummy_path = ModManager::GetDummyPath();
        n                     = FindFirstFileW(dummy_path.wstring().c_str(), lpFindFileData);
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

uint64_t ReadGameFile(anno::rdsdk::CFile* file, LPVOID lpBuffer, DWORD nNumberOfBytesToRead);
decltype(ReadGameFile)* ReadGameFile_QIP = nullptr;
uint64_t ReadGameFile(anno::rdsdk::CFile* file, LPVOID lpBuffer, DWORD nNumberOfBytesToRead)
{
    auto& file_path = file->file_path;
    if (file_path._Starts_with(L"data/config/gui/credits_")) {
        return 0;
        static auto credits_file = anno::rdsdk::CFile::ReadFile(file_path);
        if (credits_file.size() <= file->offset) {
            return 0;
        }
        memcpy(lpBuffer, credits_file.data() + file->offset, nNumberOfBytesToRead);
        file->offset += nNumberOfBytesToRead;
        return nNumberOfBytesToRead;
    }

#if defined(ADVANCED_HOOK_LOGS)
    if (!file_path.empty()) {
        spdlog::debug(L"ReadGameFile {} {}", file->file_path, nNumberOfBytesToRead);
    }
#endif

    auto mapped_path = ModManager::MapAliasedPath(file->file_path);
    if (ModManager::instance().IsFileModded(mapped_path)) {
        const auto& info = ModManager::instance().GetModdedFileInfo(mapped_path);
        if (info.is_patched) {
            auto    current_offset = file->offset;
            int64_t bytes_left_in_buffer_read_count = 0;

            if (file->size != current_offset) {
                bytes_left_in_buffer_read_count = file->size - current_offset;
            }
            if (nNumberOfBytesToRead > 0
                && nNumberOfBytesToRead < bytes_left_in_buffer_read_count) {
                bytes_left_in_buffer_read_count = nNumberOfBytesToRead;
            }
            if (file->offset > file->size) {
                return 0;
            }

#if defined(ADVANCED_HOOK_LOGS)
    if (!file_path.empty()) {
        spdlog::debug(L"ReadGameFile [f:{}|o:{}|s:{}|bl:{}]", file->file_path, file->offset, file->size, bytes_left_in_buffer_read_count);
    }
#endif

            if (bytes_left_in_buffer_read_count) {
                if (info.data.size() - file->offset < bytes_left_in_buffer_read_count) {
                    bytes_left_in_buffer_read_count = info.data.size() - file->offset;
                }
                memcpy(lpBuffer, info.data.data() + file->offset, bytes_left_in_buffer_read_count);
                file->offset += bytes_left_in_buffer_read_count;
            }

#if defined(ADVANCED_HOOK_LOGS)
    if (!file_path.empty()) {
        spdlog::debug(L"ReadGameFile [f:{}|bl:{}]", file->file_path, bytes_left_in_buffer_read_count);
    }
#endif

            return bytes_left_in_buffer_read_count;
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
            // NOTE(alexander): This is not how the game actually handles this
            // but because we are using a 'fake' 0 byte file in this case, we tell the game what
            // size it actually is, so we are guaranteed to have a buffer large enough to hold the
            // file but sometimes the games tries to query the size of that file using the 'fake'
            // file handle which because it's 0 bytes we sometimes get 0 bytes to read here
            // TODO(alexander): Hmm, the comment above doesn't really make sense here...
            if (nNumberOfBytesToRead > 0
                && nNumberOfBytesToRead < bytes_left_in_buffer_read_count) {
                bytes_left_in_buffer_read_count = nNumberOfBytesToRead;
            }
            if (bytes_left_in_buffer_read_count) {
                SetFilePointer(hFile, current_offset, NULL, FILE_BEGIN);
                DWORD         read = 0;
                ReadFile(hFile, lpBuffer, bytes_left_in_buffer_read_count, &read, NULL);
                CloseHandle(hFile);
                file->offset += read;
                return read;
            }
            CloseHandle(hFile);
            return bytes_left_in_buffer_read_count;
        }
    } else {
#if defined(ADVANCED_HOOK_LOGS)
    if (!file_path.empty()) {
        spdlog::debug(L"ReadGameFile NOT MODDED {} {}", file->file_path, nNumberOfBytesToRead);
    }
#endif

        if (file_path != L"data/config/gui/credits_eu.html") {
#if defined(ADVANCED_HOOK_LOGS)
    if (!file_path.empty()) {
        spdlog::debug(L"ReadGameFile GetFileSize");
    }
#endif
           auto size = anno::rdsdk::CFile::GetFileSize(mapped_path);
           if (size > 0 && file->file_handle) {
               size_t output_data_size = 0;
#if defined(ADVANCED_HOOK_LOGS)
    if (!file_path.empty()) {
        spdlog::debug(L"ReadGameFile ReadFileFromContainer {} {}", file->file_path, nNumberOfBytesToRead);
    }
#endif
               anno::ReadFileFromContainer(
                   *(uintptr_t*)GetAddress(anno::SOME_GLOBAL_STRUCTURE_ARCHIVE), mapped_path.wstring(),
                   (char**)&lpBuffer, &output_data_size);
               return output_data_size;
           }
        }
#if defined(ADVANCED_HOOK_LOGS)
    if (!file_path.empty()) {
        spdlog::debug(L"ReadGameFileQIP {} {}", file->file_path, nNumberOfBytesToRead);
    }
#endif
        return ReadGameFile_QIP(file, lpBuffer, nNumberOfBytesToRead);
    }
}

bool ReadInt64FromXMLNode(void* obj, const char* name, int64_t* out);
decltype(ReadInt64FromXMLNode)* ReadInt64FromXMLNode_QIP = nullptr;
bool ReadInt64FromXMLNode(void* obj, const char *name, int64_t *out)
{
    if (strcmp(name, "MinEpilepsyWarningTime") == 0) {
        ReadInt64FromXMLNode_QIP(obj, name, out);
        *out = 1;
        return true;
    }
    else {
        return ReadInt64FromXMLNode_QIP(obj, name, out);
    }
}

struct StrangePlantTreeConfig {
    char     pad[0x20];
    uint64_t growth_time; // 0x20 NOTE(alexander): Must be at least 2 and should be divisible by 2
};

// TODO(alexander): Implement something for this in meow-hook!
static bool set_import(const std::string& name, uintptr_t func)
{
    static uint64_t image_base = 0;

    if (image_base == 0) {
        image_base = uint64_t(GetModuleHandleA(NULL));
    }

    bool result    = false;
    auto dosHeader = (PIMAGE_DOS_HEADER)(image_base);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        throw std::runtime_error("Invalid DOS Signature");
    }

    auto header = (PIMAGE_NT_HEADERS)((image_base + (dosHeader->e_lfanew * sizeof(char))));
    if (header->Signature != IMAGE_NT_SIGNATURE) {
        throw std::runtime_error("Invalid NT Signature");
    }

    // BuildImportTable
    PIMAGE_DATA_DIRECTORY directory =
        &header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

    if (directory->Size > 0) {
        auto importDesc = (PIMAGE_IMPORT_DESCRIPTOR)(header->OptionalHeader.ImageBase
                                                     + directory->VirtualAddress);
        for (; !IsBadReadPtr(importDesc, sizeof(IMAGE_IMPORT_DESCRIPTOR)) && importDesc->Name;
             importDesc++) {
            wchar_t      buf[0xFFF] = {};
            auto         name2      = (LPCSTR)(header->OptionalHeader.ImageBase + importDesc->Name);
            std::wstring sname;
            size_t       converted;
            mbstowcs_s(&converted, buf, name2, sizeof(buf));
            sname       = buf;
            auto csname = sname.c_str();

            HMODULE handle = LoadLibraryW(csname);

            if (handle == nullptr) {
                SetLastError(ERROR_MOD_NOT_FOUND);
                break;
            }

            auto* thunkRef =
                (uintptr_t*)(header->OptionalHeader.ImageBase + importDesc->OriginalFirstThunk);
            auto* funcRef = (FARPROC*)(header->OptionalHeader.ImageBase + importDesc->FirstThunk);

            if (!importDesc->OriginalFirstThunk) // no hint table
            {
                thunkRef = (uintptr_t*)(header->OptionalHeader.ImageBase + importDesc->FirstThunk);
            }

            for (; *thunkRef, *funcRef; thunkRef++, (void)funcRef++) {
                if (!IMAGE_SNAP_BY_ORDINAL(*thunkRef)) {
                    std::string import =
                        (LPCSTR)
                        & ((PIMAGE_IMPORT_BY_NAME)(header->OptionalHeader.ImageBase + (*thunkRef)))
                              ->Name;

                    if (import == name) {
                        DWORD oldProtect;
                        VirtualProtect((void*)funcRef, sizeof(FARPROC), PAGE_EXECUTE_READWRITE,
                                       &oldProtect);

                        *funcRef = (FARPROC)func;

                        VirtualProtect((void*)funcRef, sizeof(FARPROC), oldProtect, &oldProtect);
                        result = true;
                    }
                }
            }
        }
    }
    return result;
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

    if (!anno::FindAddresses()) {
        printf("Find addresses...failed\n");
#define VER_FILE_DESCRIPTION_STR "Anno 1800 Mod Loader"
        std::string msg = "This version is not compatible with your current Game Version.\n\n";
        msg.append("Do you want to go to the GitHub to report an issue or check for a newer "
                   "version?\n");

        if (MessageBoxA(NULL, msg.c_str(), VER_FILE_DESCRIPTION_STR,
                        MB_ICONQUESTION | MB_YESNO | MB_SYSTEMMODAL)
            == IDYES) {
            auto result =
                ShellExecuteA(nullptr, "open", "https://github.com/xforce/anno1800-mod-loader",
                              nullptr, nullptr, SW_SHOWNORMAL);
            result = result;
            TerminateProcess(GetCurrentProcess(), 0);
        }
        spdlog::error(
            "Failed to find addresses, aborting mod loader, please create an issue on GitHub");
        return;
    }

    events.DoHooking.connect([]() {
        spdlog::debug("Patching ReadFileFromContainer");
        {
            SetAddress(anno::READ_FILE_FROM_CONTAINER,
                       uintptr_t(MH_STATIC_DETOUR(GetAddress(anno::READ_FILE_FROM_CONTAINER),
                                                  ReadFileFromContainer)));
        }

        spdlog::debug("Patching ReadGameFile");
        ReadGameFile_QIP = MH_STATIC_DETOUR(GetAddress(anno::READ_GAME_FILE), ReadGameFile);

        spdlog::debug("Patching ReadInt64FromXMLNode");
        ReadInt64FromXMLNode_QIP = MH_STATIC_DETOUR(GetAddress(anno::READ_INT64_FROM_XML_NODE), ReadInt64FromXMLNode);

        {

            spdlog::debug("Patching FileGetSize");
            SetAddress(
                anno::FILE_GET_FILE_SIZE,
                uintptr_t(MH_STATIC_DETOUR(GetAddress(anno::FILE_GET_FILE_SIZE), FileGetSize)));
        }
    });
}
