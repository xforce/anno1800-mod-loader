#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

#include "utf8.h"

namespace std
{
template <> struct equal_to<fs::path> {
   bool operator()(const fs::path &l, const fs::path &r) const {
        auto left = l.lexically_normal().wstring();
        auto ls = _wcsupr(left.data());

        auto right = r.lexically_normal().wstring();
        auto rs =_wcsupr(right.data());

        return wcscmp(ls, rs) == 0;
    }
};

} // namespace std

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
    std::unordered_map<fs::path, fs::path> file_mappings;
};
