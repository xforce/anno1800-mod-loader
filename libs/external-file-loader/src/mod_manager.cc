#include "mod_manager.h"

#include "anno/random_game_functions.h"

#include <Windows.h>

Mod& ModManager::Create(const fs::path& root)
{
    auto& mod = this->mods.emplace(root, Mod(root)).first->second;
    return mod;
}

void ModManager::GameFilesReady()
{
    // Let's start doing run-time patching of the files
    // FUCKING HYPE
    patching_file_thread = std::thread([this]() {
        std::unordered_map<fs::path, std::vector<fs::path>> modded_patchable_files;
        // Let's collect all the files first;
        for (const auto& mod : mods) {
            mod.second.ForEachFile([this, &modded_patchable_files](const fs::path &game_path, const fs::path &file_path) {
                if (IsPatchableFile(game_path)) {
                    modded_patchable_files[game_path].emplace_back(file_path);
                }
            });
        }
        // Now let's patch them, if we can
        for (auto&& modded_file : modded_patchable_files) {
            auto&& [game_path, on_disk_files] = modded_file;
            auto game_file = GetGameFile(game_path);
            if (!game_file.empty()) {
                for (auto&& on_disk_file : on_disk_files) {
                    __debugbreak();
                }
            }
        }
    });
   
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
    // If we are currently patching files
    // wait for it to finish
    if (patching_file_thread.joinable()) {
        patching_file_thread.join();
    }
    {
        std::scoped_lock lk{file_cache_mutex};
        if (file_cache.count(path) > 0) {
            return file_cache.at(path);
        }
    }
    // File not in cache, yet?

    throw std::logic_error("GetModdedFileInfo shouldn't be called on a file that is not modded");
}

bool ModManager::IsPatchableFile(const fs::path& file) const
{
    // We can only patch xml files at the moment
    // Other files have to be replaced entirely
    const auto extension = file.extension();
    return extension == ".xml";
}

std::string ModManager::GetGameFile(fs::path path) const
{
    std::string output;

    char* buffer = nullptr;
    size_t output_data_size = 0;
    if (anno::ReadFileFromContainer(*(uintptr_t*)(adjust_address(0x144EE8DF8)),
        path.wstring().c_str(), &buffer, &output_data_size)) {
        buffer = buffer;
        output = { buffer, output_data_size };

        // TODO(alexander): Move to anno api
        auto game_free = (decltype(free)*)(GetProcAddress(
            GetModuleHandleA("api-ms-win-crt-heap-l1-1-0.dll"), "free"));
        game_free(buffer);
    }
    return output;
}
