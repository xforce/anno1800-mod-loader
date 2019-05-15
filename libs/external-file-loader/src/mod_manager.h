#include "mod.h"

#include <filesystem>
#include <map>
#include <mutex>

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
        std::string data;
    };

    bool        IsFileModded(const fs::path& path) const;
    const File& GetModdedFileInfo(const fs::path& path) const;

    void GameFilesReady();

    Mod& Create(const fs::path& path);

  private:
    std::map<fs::path, Mod>            mods;
    mutable std::mutex                 file_cache_mutex;
    std::unordered_map<fs::path, File> file_cache;

    void HandleFile();
};
