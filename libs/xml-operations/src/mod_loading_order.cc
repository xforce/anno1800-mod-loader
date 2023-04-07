#include <charconv>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "mod_loading_order.h"

using namespace std;

void ForSplit(const string& text, const char* delimiter, const function<void(const string&)> forEach)
{
    size_t last = 0; size_t next = 0;
    while ((next = text.find(delimiter, last)) != string::npos) {
        if (next - last > 0) {
            forEach(text.substr(last, next - last));
        }
        last = next + 1;
    }
    if (last < text.length()) {
        forEach(text.substr(last));
    }
}

vector<string> Split(const string& text, const char* delimiter)
{
    vector<string> result;
    size_t last = 0; size_t next = 0;
    while ((next = text.find(delimiter, last)) != std::string::npos) {
        if (next - last > 0) {
            result.emplace_back(text.substr(last, next - last));
        }
        last = next + 1;
    }
    if (last < text.length()) {
        result.emplace_back(text.substr(last));
    }
    return result;
}

annomods::ModLoadingOrder::Version::Version(const std::string& version)
{
    auto split = Split(version, ".");
    if (split.size() > 0) {
        std::from_chars(split[0].data(), split[0].data() + split[0].size(), major_);
    }
    if (split.size() > 1) {
        std::from_chars(split[1].data(), split[1].data() + split[1].size(), minor_);
    }
    if (split.size() > 2) {
        std::from_chars(split[2].data(), split[2].data() + split[2].size(), patch_);
    }
}

bool annomods::ModLoadingOrder::Version::operator<(const Version& other) const
{
    if (major_ == other.major_) {
        if (minor_ == other.minor_) {
            return patch_ < other.patch_;
        }
        return minor_ < other.minor_;
    }
    return major_ < other.major_;
}

bool annomods::ModLoadingOrder::Version::operator>(const Version& other) const
{
    return other < *this;
}

class DependencyGraph {
public:
    void AddDependency(string v, string w);
    void AddPackage(const std::string& id, const std::vector<std::string>& dependencies);

    void Sort(vector<string>& sorted);

private:
    vector<string> packages_;
    map<string, list<string>> dependencies_;

    void SortRecursive(const std::string& id, set<string>& visited, vector<string>& sorted, vector<string>& postDependencies);
};

void DependencyGraph::AddDependency(string id, string dependency)
{
    if (auto package = dependencies_.find(id); package == dependencies_.end()) {
        dependencies_[id] = list<string>();
    }

    dependencies_[id].push_back(dependency);
}

void DependencyGraph::AddPackage(const std::string& id, const std::vector<std::string>& dependencies)
{
    packages_.push_back(id);
    for (auto& dep : dependencies) {
        AddDependency(id, dep);
    }
}

void DependencyGraph::SortRecursive(const string& id, set<string>& visited, vector<string>& sorted, vector<string>& postDependencies)
{
    visited.insert(id);

    for (auto& dependency : dependencies_[id]) {
        if (visited.find(dependency) == visited.end()) {
            SortRecursive(dependency, visited, sorted, postDependencies);
        }
    }

    sorted.push_back(id);
}

// Sort packages by dependencies. Packages depending on * will come last.
void DependencyGraph::Sort(vector<string>& sorted)
{
    vector<string> packages;
    for (auto& vertex : this->packages_) {
        packages.push_back(vertex);
    }

    set<string> visited;
    vector<string> postDependencies;

    // packages with dependencies first
    for (auto& package : packages) {
        if (visited.find(package) == visited.end() && !dependencies_[package].empty()) {
            SortRecursive(package, visited, sorted, postDependencies);
        }
    }

    // packages without dependencies second
    for (auto& package : packages) {
        if (visited.find(package) == visited.end() && dependencies_[package].empty()) {
            SortRecursive(package, visited, sorted, postDependencies);
        }
    }

    for (auto& package : postDependencies)
    {
        sorted.push_back(package);
    }
}

void annomods::ModLoadingOrder::AddMod(const std::string& path,
    const std::string& name,
    const std::string& id,
    const std::string& version,
    const std::vector<std::string>&       deprecationIDs,
    const std::vector<std::string>& dependencyIDs)
{
    Version parsedVersion = version;

    const auto& duplicate = mods_.find(id);
    if (duplicate != mods_.end()) {
        if (duplicate->second.version > parsedVersion) {
            // skip older versions
            return;
        }
    }

    bool loadLast = dependencyIDs.end() != std::find(dependencyIDs.begin(), dependencyIDs.end(), "*");

    mods_[id] = Mod{ path, name, id, parsedVersion, dependencyIDs, loadLast };
    for (auto& alias : deprecationIDs) {
        this->aliases_.insert({ alias, id });
    }
}

void annomods::ModLoadingOrder::Sort(vector<ModLoadingOrder::Mod>& ordered)
{
    HandleDeprecation();

    multimap<string, Mod*> alphabetical;
    for (auto& mod : mods_) {
        alphabetical.emplace(mod.second.name, &mod.second);
    }

    DependencyGraph graph;
    for (auto& mod : alphabetical) {
        if (!mod.second->loadLast) {
            graph.AddPackage(mod.second->id, mod.second->dependencyIDs);
        }
    }

    DependencyGraph loadLast;
    for (auto& mod : mods_) {
        if (mod.second.loadLast) {
            loadLast.AddPackage(mod.second.id, mod.second.dependencyIDs);
        }
    }

    // mods with normal dependencies
    vector<string> sorted;
    graph.Sort(sorted);
    for (auto& id : sorted) {
        const auto& mod = mods_.find(id);
        if (mod != mods_.end() && !mod->second.loadLast) {
            ordered.push_back(mod->second);
        }
    }

    // mods with * (load last) dependencies
    sorted.clear();
    loadLast.Sort(sorted);
    for (auto& id : sorted) {
        const auto& mod = mods_.find(id);
        if (mod != mods_.end() && mod->second.loadLast) {
            ordered.push_back(mod->second);
        }
    }
}

void annomods::ModLoadingOrder::HandleDeprecation()
{
    std::map<string, Mod> filtered;
    for (auto& mod : mods_) {
        if (IsDeprecated(mod.second.id)) continue;

        filtered[mod.first] = mod.second;
    }

    mods_ = filtered;
}

bool annomods::ModLoadingOrder::IsDeprecated(const std::string& id) const
{
    return (aliases_.end() != std::find_if(aliases_.begin(), aliases_.end(), [&id](pair<string, string> x) { return x.first == id; }));
}
