#include "mod.h"

Mod::Mod(const fs::path& root)
    : root_path(root)
{
    // We have a mods directory
    std::vector<fs::path> mod_roots;
    for (const auto& file : fs::recursive_directory_iterator(root_path)) {
        if (file.is_regular_file()) {
            try {
                const auto game_path = fs::relative(fs::canonical(file), fs::canonical(root_path));
                file_mappings[game_path] = fs::canonical(file);
                // TODO(alexander): Logging
            } catch (const fs::filesystem_error& error) {
                // TODO(alexander): Logging
            }
        }
    }
}

bool Mod::HasFile(const fs::path& file) const
{
    // TODO(alexander): Do we need a mutex here?
    return file_mappings.count(file) > 0;
}

void Mod::ForEachFile(std::function<void(const fs::path&, const fs::path&)> fn) const
{
    std::for_each(std::begin(file_mappings), std::end(file_mappings), [&fn](auto &&it) {
        auto &&[game_path, file_path] = it;
        fn(game_path, file_path);
    });
}
