#include "mod.h"

#include "nlohmann/json.hpp"

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

    static fs::path GetModsDirectory();
    static fs::path GetCacheDirectory();

    bool        IsFileModded(const fs::path& path) const;
    const File& GetModdedFileInfo(const fs::path& path) const;
    void        GameFilesReady();
    Mod&        Create(const fs::path& path);

  private:
    bool        IsPatchableFile(const fs::path& file) const;
    std::string ReadGameFile(fs::path path) const;
    void        CollectPatchableFiles();

    // Cache system stuff
    // This should be moved into it's own class
    std::string                GetFileHash(const fs::path& file) const;
    std::string                GetDataHash(const std::string& data) const;
    void                       ReadCache();
    std::optional<std::string> CheckCacheLayer(const fs::path&    game_path,
                                               const std::string& input_hash,
                                               const std::string& patch_hash);
    std::string ReadCacheLayer(const fs::path& game_path, const std::string& input_hash);
    std::string PushCacheLayer(const fs::path& game_path, const std::string& last_valid_cache,
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

    std::vector<Mod>                                      mods;
    mutable std::mutex                                    file_cache_mutex;
    std::unordered_map<fs::path, File>                    file_cache;
    std::unordered_map<fs::path, std::vector<fs::path>>   modded_patchable_files_;
    std::unordered_map<fs::path, std::vector<CacheLayer>> modded_file_cache_info_;
    mutable std::thread                                   patching_file_thread;
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
