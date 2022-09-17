#pragma once

#include "mod.h"

#include "nlohmann/json.hpp"

#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

namespace fs = std::filesystem;

class ModManager
{
  public:
    static ModManager& instance()
    {
        static ModManager instance;
        return instance;
    }
    struct File {
        size_t      size;
        bool        is_patched = false;
        std::string data;
        fs::path    disk_path;
    };

    ~ModManager();

    void Shutdown();

    static fs::path GetModsDirectory();
    static fs::path GetCacheDirectory();
    static fs::path GetDummyPath();
    static void     EnsureDummy();

    bool                            IsFileModded(const fs::path& path) const;
    const File&                     GetModdedFileInfo(const fs::path& path) const;
    void                            GameFilesReady();
    Mod&                            Create(const fs::path& path);
    void                            LoadMods();
    const std::vector<std::string>& GetPythonScripts() const;

    static std::string ReadGameFile(fs::path path);

    static fs::path MapAliasedPath(fs::path path);

  private:
    bool IsPatchableFile(const fs::path& file) const;
    bool IsIncludeFile(const fs::path& file) const;
    bool IsPythonStartScript(const fs::path& file) const;
    void CollectPatchableFiles();
    void StartWatchingFiles();
    void WaitModsReady() const;
    Mod& GetModContainingFile(const fs::path& file);

    // Cache system stuff
    // This should be moved into it's own class
    struct LayerId {
        std::string output;
        std::string patch;
    };

    std::string                GetFileHash(const fs::path& file) const;
    std::string                GetDataHash(const std::string& data) const;
    void                       ReadCache();
    std::optional<std::string> CheckCacheLayer(const fs::path&    game_path,
                                               const std::string& input_hash,
                                               const std::string& patch_hash);
    std::string ReadCacheLayer(const fs::path& game_path, const std::string& input_hash);
    LayerId PushCacheLayer(const fs::path& game_path, const LayerId& last_valid_cache,
                               const std::string& patch_file_hash, const std::string& buf,
                               const std::string& mod_name = "");
    void        WriteCacheInfo(const fs::path& game_path);

    struct CacheLayer {
        std::string input_hash;
        std::string patch_hash;
        std::string output_hash;
        std::string layer_file;
        std::string mod_name;
    };
    friend void to_json(nlohmann::json& j, const ModManager::CacheLayer& p);
    friend void from_json(const nlohmann::json& j, ModManager::CacheLayer& p);

    std::vector<Mod>                                      mods_;
    std::vector<std::string>                              python_scripts_;
    mutable std::mutex                                    file_cache_mutex_;
    std::unordered_map<fs::path, File>                    file_cache_;
    std::unordered_map<fs::path, std::vector<fs::path>>   modded_patchable_files_;
    std::unordered_map<fs::path, std::vector<CacheLayer>> modded_file_cache_info_;
    mutable std::thread                                   patching_file_thread_;
    mutable std::thread                                   watch_file_thread_;
    OVERLAPPED                                            watch_file_ov_;
    mutable std::thread                                   reload_mods_thread_;
    std::atomic_bool                                      mods_change_wile_reload_ = false;
    mutable std::condition_variable                       mods_ready_cv_;
    mutable std::mutex                                    mods_ready_mx_;
    std::atomic_bool                                      mods_ready_     = false;
    std::atomic_bool                                      shuttding_down_ = false;
};

inline void to_json(nlohmann::json& j, const ModManager::CacheLayer& p)
{
    j = nlohmann::json{{"input_hash", p.input_hash},
                       {"patch_hash", p.patch_hash},
                       {"output_hash", p.output_hash},
                       {"layer_file", p.layer_file},
                       {"mod_name", p.mod_name}};
}

inline void from_json(const nlohmann::json& j, ModManager::CacheLayer& p)
{
    j.at("input_hash").get_to(p.input_hash);
    j.at("patch_hash").get_to(p.patch_hash);
    j.at("output_hash").get_to(p.output_hash);
    j.at("layer_file").get_to(p.layer_file);
    j.at("mod_name").get_to(p.mod_name);
}
