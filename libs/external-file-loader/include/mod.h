#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>

#include "fs.h"

class Mod
{
  public:
    Mod() = default;
    explicit Mod(const fs::path &root);

    std::string Name() const;
    bool        HasFile(const fs::path &file) const;
    void        ForEachFile(std::function<void(const fs::path &, const fs::path &)>) const;
    fs::path    Path() const;

  private:
    fs::path                               root_path;
    PathMap<fs::path> file_mappings;
};
