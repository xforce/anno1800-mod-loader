#include "external-file-loader.h"
#include "mod.h"
#include "mod_manager.h"

#include "anno/random_game_functions.h"
#include "hooking.h"

#include "spdlog/spdlog.h"

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

class Mod;

std::vector<Mod> mods;

uintptr_t* ReadFileFromContainerOIP = nullptr;
bool __fastcall ReadFileFromContainer(__int64 archive_file_map, const std::wstring& file_path,
                                      char** output_data_pointer, size_t* output_data_size)
{
    // archive_file_map is a pointer to a struct identifying which rda this file resides in
    // as each rda is actually just a memory mapped file
    // but we don't care about that at the moment, probably never will
    if (ModManager::instance().IsFileModded(file_path)) {
        auto info = ModManager::instance().GetModdedFileInfo(file_path);
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
            ReadFile(hFile, *output_data_pointer, lFileSize.QuadPart, &read, NULL);
            CloseHandle(hFile);
        }
        return true;
    }
    return anno::ReadFileFromContainer(archive_file_map, file_path, output_data_pointer,
                                       output_data_size);
}

bool GetContainerBlockInfo(uintptr_t* a1, const std::wstring& file_path, int a3)
{
    if (file_path.find(L"checksum.db") != std::wstring::npos) {
        ModManager::instance().GameFilesReady();
    }
    // Call the original GetContainerBlockInfo
    // '13.12551.0.1532' === 1453972A0
    // '13.12551.0.1532' === 145394060
    auto result = anno::GetContainerBlockInfo(a1, file_path, a3);
    if (ModManager::instance().IsFileModded(file_path)) {
        auto info = ModManager::instance().GetModdedFileInfo(file_path);
        // TODO(alexander): Move this 'a1 + 0x88' to some nice structure in an API
        // package
        *(size_t*)((char*)a1 + 0x88) = info.size;
    }
    return result;
}
static bool IsModEnabled(fs::path path)
{
    // If mod folder name starts with '-', we don't enable it.
    return path.stem().wstring().find(L'-') != 0;
}
void EnableExtenalFileLoading(Events& events)
{
    auto mods_directory = ModManager::GetModsDirectory();
    // We have a mods directory
    // Now create a mod for each of these
    std::vector<fs::path> mod_roots;
    for (auto&& root : fs::directory_iterator(mods_directory)) {
        if (root.is_directory() && IsModEnabled(root.path())) {
            ModManager::instance().Create(root.path());
        }
        else {
            spdlog::info("Disabled mod {}", root.path().stem().string());
        }
    }

    events.DoHooking.connect([]() {
        anno::SetAddress(anno::READ_FILE_FROM_CONTAINER,
                         uintptr_t(detour_func(GetAddress(anno::READ_FILE_FROM_CONTAINER),
                                               ReadFileFromContainer)));
        anno::SetAddress(anno::GET_CONTAINER_BLOCK_INFO,
                         uintptr_t(detour_func(GetAddress(anno::GET_CONTAINER_BLOCK_INFO),
                                               GetContainerBlockInfo)));

        // retn(0x148423610); // Disables UI rendering...
    });
}
