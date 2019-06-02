#include "mod.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <thread>

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

    bool        IsFileModded(const fs::path& path) const;
    const File& GetModdedFileInfo(const fs::path& path) const;
    void        GameFilesReady();
    Mod&        Create(const fs::path& path);

  private:
    bool        IsPatchableFile(const fs::path& file) const;
    std::string GetFileHash(const fs::path& file) const;
    std::string GetDataHash(const std::string& data) const;
    std::string GetGameFile(fs::path path) const;

    std::map<fs::path, Mod>            mods;
    mutable std::mutex                 file_cache_mutex;
    std::unordered_map<fs::path, File> file_cache;
    mutable std::thread                patching_file_thread;
};
