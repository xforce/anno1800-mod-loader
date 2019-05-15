#include "mod_manager.h"

#include "anno/random_game_functions.h"

#include <Windows.h>

Mod& ModManager::Create(const fs::path& root)
{
    auto& mod = this->mods.emplace(root, Mod(root)).first->second;
    // mod.ForEachFile([]() {

    // });
    return mod;
}

void ModManager::GameFilesReady()
{
    char*  buffer           = nullptr;
    size_t output_data_size = 0;
    anno::ReadFileFromContainer(*(uintptr_t*)(adjust_address(0x144EE8DF8)),
                                L"data/config/game/camera.xml", &buffer, &output_data_size);
    buffer         = buffer;
    auto game_free = (decltype(free)*)(GetProcAddress(
        GetModuleHandleA("api-ms-win-crt-heap-l1-1-0.dll"), "free"));
    game_free(buffer);
}

bool ModManager::IsFileModded(const fs::path& path) const
{
    for (const auto& mod : mods) {
        if (mod.second.HasFile(path.lexically_normal())) {
            return true;
        }
    }
    return false;
}

const ModManager::File& ModManager::GetModdedFileInfo(const fs::path& path) const
{
    {
        std::scoped_lock lk{file_cache_mutex};
        if (file_cache.count(path) > 0) {
            return file_cache.at(path);
        }
    }
    // File not in cache, yet?

    throw std::logic_error("GetModdedFileInfo shouldn't be called on a file that is not modded");
}
