#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

#include "utf8.h"

struct fs_hash {
    size_t operator()(const fs::path& x) const
    {
        auto c = x.lexically_normal().wstring();
        auto s = _wcsupr(c.data());

        return fs::hash_value(s);
    }
};

struct fs_equal_to {
    bool operator()(const fs::path& l, const fs::path& r) const {
        auto left = l.lexically_normal().wstring();
        auto ls = _wcsupr(left.data());

        auto right = r.lexically_normal().wstring();
        auto rs = _wcsupr(right.data());

        return wcscmp(ls, rs) == 0;
    }
};

template<typename T>
using PathMap = std::unordered_map<fs::path, T, fs_hash, fs_equal_to>;