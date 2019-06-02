#include "mod_manager.h"

#include "anno/random_game_functions.h"
#include "xml_operations.h"

#include "absl/strings/str_cat.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

// Prevent preprocess errors with boringssl
#undef X509_NAME
#undef X509_CERT_PAIR
#undef X509_EXTENSIONS
#include "openssl/sha.h"

#include <Windows.h>

#include <fstream>

constexpr static auto PATCH_OP_VERSION = "1.1";

Mod& ModManager::Create(const fs::path& root)
{
    spdlog::info("Loading mod {}", root.stem().string());
    auto& mod = this->mods.emplace(root, Mod(root)).first->second;
    return mod;
}

struct CacheLayer {
    std::string input_hash;
    std::string patch_hash;
    std::string output_hash;
    std::string layer_file;

    std::string CacheFileName() const
    {
        if (!patch_hash.empty()) {
            return patch_hash;
        }
        return output_hash;
    }
};

void to_json(nlohmann::json& j, const CacheLayer& p)
{
    j = nlohmann::json{{"input_hash", p.input_hash},
                       {"patch_hash", p.patch_hash},
                       {"output_hash", p.output_hash},
                       {"layer_file", p.layer_file}};
}

void from_json(const nlohmann::json& j, CacheLayer& p)
{
    j.at("input_hash").get_to(p.input_hash);
    j.at("patch_hash").get_to(p.patch_hash);
    j.at("output_hash").get_to(p.output_hash);
    j.at("layer_file").get_to(p.layer_file);
}

void ModManager::GameFilesReady()
{
    // Let's start doing run-time patching of the files
    // FUCKING HYPE
    patching_file_thread = std::thread([this]() {
        // TODO(alexander): refactor this
        std::unordered_map<fs::path, std::vector<fs::path>> modded_patchable_files;
        // Let's collect all the files first;
        for (const auto& mod : mods) {
            mod.second.ForEachFile([this, &modded_patchable_files](const fs::path& game_path,
                                                                   const fs::path& file_path) {
                if (IsPatchableFile(game_path)) {
                    modded_patchable_files[game_path].emplace_back(file_path);
                } else {
                    auto hFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        LARGE_INTEGER lFileSize;
                        GetFileSizeEx(hFile, &lFileSize);
                        CloseHandle(hFile);
                        file_cache[game_path] = {
                            static_cast<size_t>(lFileSize.QuadPart), false, {}, file_path};
                    }
                }
            });
        }

        std::unordered_map<fs::path, std::vector<CacheLayer>> modded_file_cache_info;

        // Read the caching information
        const auto cache_directory = ModManager::GetModsDirectory() / ".cache";
        for (auto&& modded_file : modded_patchable_files) {
            auto [game_path, on_disk_files] = modded_file;
            fs::create_directories(cache_directory / game_path);
            auto json_path = cache_directory / game_path;
            json_path += ".json";
            if (fs::exists(json_path)) {
                std::ifstream ifs(json_path);
                try {
                    const auto&              data        = nlohmann::json::parse(ifs);
                    std::vector<std::string> layer_order = data["layers"]["order"];
                    std::string patch_op_version         = data.at("version").get<std::string>();
                    if (PATCH_OP_VERSION == patch_op_version) {
                        std::vector<CacheLayer> cache_layers;
                        for (auto&& layer : layer_order) {
                            cache_layers.emplace_back(data["layers"].at(layer).get<CacheLayer>());
                        }
                        modded_file_cache_info[game_path] = std::move(cache_layers);
                    }
                } catch (const nlohmann::json::exception&) {
                }
            }
        }

        // Now let's patch them, if we can
        for (auto&& modded_file : modded_patchable_files) {
            auto&& [game_path, on_disk_files] = modded_file;

            auto game_file = GetGameFile(game_path);
            auto cache_end = end(modded_file_cache_info[game_path]);
            // Prepare first cache layer

            if (!game_file.empty()) {
                auto cache_it = begin(modded_file_cache_info[game_path]);

                game_file          = "<MEOW_XML_SUCKS>" + game_file + "</MEOW_XML_SUCKS>";
                auto starting_hash = GetDataHash(game_file);

                // Only read xml once we have a cache miss
                xmlDocPtr game_xml = nullptr;
                if (cache_end == cache_it || cache_it->output_hash != starting_hash) {
                    game_xml = xmlReadMemory(game_file.data(), game_file.size(), "", "UTF-8",
                                             XML_PARSE_RECOVER);
                    modded_file_cache_info[game_path].clear();
                    modded_file_cache_info[game_path].push_back({"", "", starting_hash, ""});
                    cache_end = end(modded_file_cache_info[game_path]);
                    cache_it  = begin(modded_file_cache_info[game_path]);
                }
                for (auto&& on_disk_file : on_disk_files) {
                    auto patch_file_hash = GetFileHash(on_disk_file);
                    auto prev_cache_it   = cache_it;
                    if (cache_end != cache_it) {
                        ++cache_it;
                    }
                    if (cache_end == cache_it || cache_it->patch_hash != patch_file_hash) {

                        // Read previous cached file if there was any
                        if (prev_cache_it != cache_end && !game_xml) {
                            const auto cache_file_path =
                                (cache_directory / game_path / prev_cache_it->CacheFileName());
                            std::ifstream   file(cache_file_path, std::ios::binary | std::ios::ate);
                            std::streamsize size = file.tellg();
                            file.seekg(0, std::ios::beg);
                            std::string buffer;
                            buffer.resize(size);
                            if (file.read(buffer.data(), size)) {
                                game_xml = xmlReadMemory(buffer.data(), buffer.size(), "", "UTF-8",
                                                         XML_PARSE_RECOVER);
                            }
                        }

                        // Check if we have a cached file for this...
                        auto operations = XmlOperation::GetXmlOperationsFromFile(on_disk_file);
                        for (auto&& operation : operations) {
                            operation.Apply(game_xml);
                        }

                        xmlChar* xmlbuff;
                        int      buffersize;
                        xmlDocDumpFormatMemory(game_xml, &xmlbuff, &buffersize, 1);
                        std::string buf = (const char*)(xmlbuff);
                        buf = buf.substr(buf.find("<MEOW_XML_SUCKS>") + strlen("<MEOW_XML_SUCKS>"));
                        buf = buf.substr(0, buf.find("</MEOW_XML_SUCKS>"));

                        fs::create_directories(cache_directory / game_path);
                        std::ofstream myfile;
                        myfile.open((cache_directory / game_path / patch_file_hash));
                        myfile.write(buf.data(), buf.size());
                        myfile.close();
                        modded_file_cache_info[game_path].push_back(
                            {"", patch_file_hash, GetDataHash(buf), patch_file_hash});
                        cache_end = end(modded_file_cache_info[game_path]);
                        cache_it  = begin(modded_file_cache_info[game_path]);
                    }
                }
                // Everything was a cache hit
                if (!game_xml) {
                    // We check for at least 2 cache layers here
                    // as the game input is the first cache layer
                    if (cache_end != cache_it && modded_file_cache_info[game_path].size() > 1) {
                        //
                        auto cache_file_name =
                            modded_file_cache_info[game_path]
                                                  [modded_file_cache_info[game_path].size() - 1]
                                                      .CacheFileName();
                        std::ifstream   file((cache_directory / game_path / cache_file_name),
                                           std::ios::binary | std::ios::ate);
                        std::streamsize size = file.tellg();
                        file.seekg(0, std::ios::beg);
                        std::string buffer;
                        buffer.resize(size);
                        if (file.read(buffer.data(), size)) {
                            file_cache[game_path] = {buffer.size(), true, buffer};
                        }
                    } else {
                        game_file = game_file.substr(game_file.find("<MEOW_XML_SUCKS>")
                                                     + strlen("<MEOW_XML_SUCKS>"));
                        game_file = game_file.substr(0, game_file.find("</MEOW_XML_SUCKS>"));
                        file_cache[game_path] = {game_file.size(), true, game_file};
                    }

                } else {
                    xmlChar* xmlbuff;
                    int      buffersize;
                    xmlDocDumpFormatMemory(game_xml, &xmlbuff, &buffersize, 1);
                    std::string buf = (const char*)(xmlbuff);
                    buf = buf.substr(buf.find("<MEOW_XML_SUCKS>") + strlen("<MEOW_XML_SUCKS>"));
                    buf = buf.substr(0, buf.find("</MEOW_XML_SUCKS>"));
                    file_cache[game_path] = {buf.size(), true, buf};
                }

                // Write cache info
                auto json_path = cache_directory / game_path;
                json_path += ".json";
                std::ofstream            ofs(json_path);
                nlohmann::json           j;
                std::vector<std::string> order;
                for (auto& layer : modded_file_cache_info[game_path]) {
                    order.push_back(layer.output_hash);
                    j["layers"][layer.output_hash] = layer;
                }
                j["layers"]["order"] = order;
                j["version"]         = PATCH_OP_VERSION;
                ofs << j;
                ofs.close();

                for (auto file : fs::directory_iterator(cache_directory / game_path)) {
                    const auto file_name = file.path().filename();
                    auto       it        = std::find_if(
                        begin(modded_file_cache_info[game_path]),
                        end(modded_file_cache_info[game_path]),
                        [file_name](const auto& x) { return file_name == x.CacheFileName(); });
                    //
                    if (it == end(modded_file_cache_info[game_path])) {
                        fs::remove(file);
                    }
                }
                // Let's clean up old cache files
            } else {
                spdlog::error("Failed to get original game file {}", game_path.string());
            }
        }
    });
}

bool ModManager::IsFileModded(const fs::path& path) const
{
    for (const auto& mod : mods) {
        if (mod.second.HasFile(path.lexically_normal())) {
            return true;
        }
    }
    return false;
}

const ModManager::File& ModManager::GetModdedFileInfo(const fs::path& path) const
{
    // If we are currently patching files
    // wait for it to finish
    if (patching_file_thread.joinable()) {
        patching_file_thread.join();
    }
    {
        std::scoped_lock lk{file_cache_mutex};
        if (file_cache.count(path) > 0) {
            return file_cache.at(path);
        }
    }
    // File not in cache, yet?
    throw std::logic_error("GetModdedFileInfo shouldn't be called on a file that is not modded");
}

fs::path ModManager::GetModsDirectory()
{
    fs::path mods_directory;
    HMODULE  module;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPWSTR)&ModManager::GetModsDirectory, &module)) {
        WCHAR path[0x7FFF] = {}; // Support for long paths, in theory
        GetModuleFileNameW(module, path, sizeof(path));
        fs::path dll_file(path);
        try {
            mods_directory = fs::canonical(dll_file.parent_path() / ".." / ".." / "mods");
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to get current module directory {}", e.what());
            return {};
        }
    } else {
        spdlog::error("Failed to get current module directory {}", GetLastError());
        return {};
    }
    return mods_directory;
}

bool ModManager::IsPatchableFile(const fs::path& file) const
{
    // We can only patch xml files at the moment
    // Other files have to be replaced entirely
    const auto extension = file.extension();
    return extension == ".xml";
}

std::string ModManager::GetFileHash(const fs::path& path) const
{
    std::ifstream   file(path, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string buffer;
    buffer.resize(size);
    if (file.read(buffer.data(), size)) {
        return GetDataHash(buffer);
    }
    throw new std::runtime_error("Failed to read file");
    return {};
}

std::string ModManager::GetDataHash(const std::string& data) const
{
    uint8_t digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const uint8_t*>(data.data()), data.size(), digest);
    std::string result;
    for (auto& n : digest) {
        absl::StrAppend(&result, absl::Hex(n, absl::kZeroPad2));
    }
    return result;
}

std::string ModManager::GetGameFile(fs::path path) const
{
    std::string output;

    char*  buffer           = nullptr;
    size_t output_data_size = 0;
    if (anno::ReadFileFromContainer(*(uintptr_t*)GetAddress(anno::SOME_GLOBAL_STRUCTURE_ARCHIVE),
                                    path.wstring().c_str(), &buffer, &output_data_size)) {
        buffer = buffer;
        output = {buffer, output_data_size};

        // TODO(alexander): Move to anno api
        auto game_free = (decltype(free)*)(GetProcAddress(
            GetModuleHandleA("api-ms-win-crt-heap-l1-1-0.dll"), "free"));
        game_free(buffer);
    }
    return output;
}
