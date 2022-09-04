#include "mod.h"


#include <fstream>
#include <sstream>


Mod::Mod(const fs::path& root)
    : root_path(root)
{
    modID = "";
    modVer = "";
    modGUID = "";

    // We have a mods directory
    std::vector<fs::path> mod_roots;
    for (const auto& file : fs::recursive_directory_iterator(root_path)) {
        if (file.is_regular_file()) {
            try {
                const auto game_path = fs::relative(fs::canonical(file), fs::canonical(root_path));
                file_mappings[game_path] = fs::canonical(file);
            } catch (const fs::filesystem_error& error) {
                // TODO(alexander): Logging
            }
        }
    }

    //file_mappings is case sensitive - fs::exists will be windows style case insesnitive
    auto modInfo = root_path / "modinfo.json";
    if(fs::exists(modInfo)){
        ParseInfoJSON(modInfo);
    }
}


std::string Mod::ID() const
{
    return modID;
}

std::string Mod::GUID() const
{
    return modGUID;
}

std::string Mod::Name() const
{
    return root_path.stem().string();
}

std::string Mod::Version() const
{
    return modVer;
}

std::string Mod::Info() const
{
    return InfoJSON().dump();
}

nlohmann::json Mod::InfoJSON() const
{
    nlohmann::json info = {
        {"ID", this->ID()},
        {"GUID", this->GUID()},
        {"Version", this->Version()},
        {"Name", this->Name()}
    };
    return info;
}

bool Mod::HasFile(const fs::path& file) const
{
    return file_mappings.count(file) > 0;
}

void Mod::ForEachFile(std::function<void(const fs::path&, const fs::path&)> fn) const
{
    std::for_each(std::begin(file_mappings), std::end(file_mappings), [&fn](auto&& it) {
        auto&& [game_path, file_path] = it;
        fn(game_path, file_path);
    });
}

fs::path Mod::Path() const
{
    return root_path;
}

void Mod::ParseInfoJSON(const fs::path& json_path)
{
     if (fs::exists(json_path)) {
        std::ifstream ifs(json_path);
        try {
            const auto&  data = nlohmann::json::parse(ifs);
            if(data.find("ModID") != data.end()){
                modID = data.at("ModID").get<std::string>();
            }
            if(data.find("GUID") != data.end()){
                modGUID = data.at("GUID").get<std::string>();
            }
            if(data.find("Version") != data.end()){
                modVer = data.at("Version").get<std::string>();
            }
        } catch (const nlohmann::json::exception&) {
        }
    }
}
