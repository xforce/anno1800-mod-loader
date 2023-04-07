#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace annomods
{

class ModLoadingOrder
{
public:
    ModLoadingOrder() = default;
    virtual ~ModLoadingOrder() = default;

    class Version
    {
    public:
        Version() = default;
        Version(const std::string& version);
        virtual ~Version() = default;

        bool operator<(const Version& other) const;
        bool operator>(const Version& other) const;

    private:
        uint32_t major_ = 0;
        uint32_t minor_ = 0;
        uint32_t patch_ = 0;
    };

    struct Mod
    {
        std::string path;
        std::string name;
        std::string id;
        Version version;
        std::vector<std::string> dependencyIDs;
        bool loadLast = false;
    };

    void AddMod(const std::string& path,
        const std::string& name,
        const std::string& id,
        const std::string& version,
        const std::vector<std::string>& deprecationIDs,
        const std::vector<std::string>& dependencyIDs);

    void Sort(std::vector<ModLoadingOrder::Mod>& ordered);

    void HandleDeprecation();

    [[nodiscard]] bool IsDeprecated(const std::string& id) const;

private:
    std::map<std::string, Mod> mods_;
    std::multimap<std::string, std::string> aliases_;
};

}
