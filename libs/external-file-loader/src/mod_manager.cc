#include "mod_manager.h"

#include "anno/random_game_functions.h"
#include "xml_operations.h"

#include "absl/strings/str_cat.h"
#include "libxml/parser.h"
#include "libxml/tree.h"
#include "spdlog/spdlog.h"

// Prevent preprocess errors with boringssl
#undef X509_NAME
#undef X509_CERT_PAIR
#undef X509_EXTENSIONS
#include "openssl/sha.h"

#include <Windows.h>
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8
#include <compressapi.h>
#pragma comment(lib, "Cabinet.lib")

#include <fstream>
#include <optional>

constexpr static auto PATCH_OP_VERSION = "1.1";

Mod& ModManager::Create(const fs::path& root)
{
    spdlog::info("Loading mod {}", root.stem().string());
    auto& mod = this->mods.emplace_back(root);
    return mod;
}

void ModManager::CollectPatchableFiles()
{
    for (const auto& mod : mods) {
        mod.ForEachFile([this](const fs::path& game_path, const fs::path& file_path) {
            if (IsPatchableFile(game_path)) {
                modded_patchable_files_[game_path].emplace_back(file_path);
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
}

void ModManager::ReadCache()
{
    const auto cache_directory = ModManager::GetCacheDirectory();
    for (auto&& modded_file : modded_patchable_files_) {
        auto [game_path, on_disk_files] = modded_file;
        fs::create_directories(cache_directory / game_path);
        auto json_path = cache_directory / game_path;
        json_path += ".json";
        if (fs::exists(json_path)) {
            std::ifstream ifs(json_path);
            try {
                const auto&              data             = nlohmann::json::parse(ifs);
                std::vector<std::string> layer_order      = data["layers"]["order"];
                std::string              patch_op_version = data.at("version").get<std::string>();
                if (PATCH_OP_VERSION == patch_op_version) {
                    std::vector<CacheLayer> cache_layers;
                    for (auto&& layer : layer_order) {
                        cache_layers.emplace_back(data["layers"].at(layer).get<CacheLayer>());
                    }
                    modded_file_cache_info_[game_path] = std::move(cache_layers);
                }
            } catch (const nlohmann::json::exception&) {
            }
        }
    }
}

void ModManager::WriteCacheInfo(const fs::path& game_path)
{
    const auto cache_directory = ModManager::GetCacheDirectory();

    auto json_path = cache_directory / game_path;
    json_path += ".json";
    std::ofstream            ofs(json_path);
    nlohmann::json           j;
    std::vector<std::string> order;
    for (auto& layer : modded_file_cache_info_[game_path]) {
        order.push_back(layer.output_hash);
        j["layers"][layer.output_hash] = layer;
    }
    j["layers"]["order"] = order;
    j["version"]         = PATCH_OP_VERSION;
    ofs << j;
    ofs.close();

    // Let's clean up old cache files
    for (auto file : fs::directory_iterator(cache_directory / game_path)) {
        const auto file_name = file.path().filename();
        auto       it        = std::find_if(begin(modded_file_cache_info_[game_path]),
                               end(modded_file_cache_info_[game_path]),
                               [file_name](const auto& x) { return file_name == x.layer_file; });
        //
        if (it == end(modded_file_cache_info_[game_path])) {
            fs::remove(file);
        }
    }
}

std::optional<std::string> ModManager::CheckCacheLayer(const fs::path&    game_path,
                                                       const std::string& input_hash,
                                                       const std::string& patch_hash)
{
    if (input_hash.empty()) {
        return {};
    }

    for (auto&& cache : modded_file_cache_info_[game_path]) {
        if (cache.input_hash == input_hash && cache.patch_hash == patch_hash) {
            return cache.output_hash;
        }
    }
    return {};
}

std::string ModManager::ReadCacheLayer(const fs::path& game_path, const std::string& input_hash)
{
    const auto cache_directory = ModManager::GetCacheDirectory();

    for (auto&& cache : modded_file_cache_info_[game_path]) {
        if (cache.output_hash == input_hash) {
            const auto      cache_file      = cache.layer_file;
            const auto      cache_file_path = (cache_directory / game_path / cache_file);
            std::ifstream   file(cache_file_path, std::ios::binary | std::ios::ate);
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::string buffer;
            buffer.resize(size);
            if (file.read(buffer.data(), size)) {
                DECOMPRESSOR_HANDLE Decompressor = NULL;
                SIZE_T              DecompressedBufferSize;
                SIZE_T              DecompressedDataSize;
                BOOL                Success;
                Success = CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, NULL, &Decompressor);
                DWORD level = 1;
                SetDecompressorInformation(Decompressor, COMPRESS_INFORMATION_CLASS_LEVEL, &level,
                                           sizeof(level));
                Success = Decompress(Decompressor, buffer.data(), buffer.size(), NULL, 0,
                                     &DecompressedBufferSize);
                std::string output;
                output.resize(DecompressedBufferSize);
                Success = Decompress(Decompressor, buffer.data(), buffer.size(), output.data(),
                                     output.size(), &DecompressedDataSize);
                CloseDecompressor(Decompressor);
                output.resize(DecompressedDataSize);
                return output;
            }
        }
    }
    return "";
}

std::string ModManager::PushCacheLayer(const fs::path&    game_path,
                                       const std::string& last_valid_cache,
                                       const std::string& patch_file_hash, const std::string& buf,
                                       const std::string& mod_name)
{
    CacheLayer layer;
    layer.input_hash  = last_valid_cache;
    layer.output_hash = GetDataHash(buf);
    layer.patch_hash  = patch_file_hash;
    layer.layer_file  = layer.output_hash;
    layer.mod_name    = mod_name;

    auto& cache = modded_file_cache_info_[game_path];

    auto it = find_if(begin(cache), end(cache), [&last_valid_cache](const auto& x) {
        return x.output_hash == last_valid_cache;
    });
    if (it == end(cache)) {
        cache.clear();
    } else {
        std::advance(it, 1);
        cache.erase(it, end(cache));
    }

    const auto cache_directory = ModManager::GetCacheDirectory();

    fs::create_directories(cache_directory / game_path);
    std::ofstream ofs((cache_directory / game_path / layer.layer_file), std::ofstream::binary);

    COMPRESSOR_HANDLE Compressor = NULL;
    SIZE_T            CompressedBufferSize;
    SIZE_T            CompressedDataSize;
    BOOL              Success;
    Success     = CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, NULL, &Compressor);
    DWORD level = 1;
    SetCompressorInformation(Compressor, COMPRESS_INFORMATION_CLASS_LEVEL, &level, sizeof(level));
    Success = Compress(Compressor, buf.data(), buf.size(), NULL, 0, &CompressedBufferSize);
    std::string CompressedBuffer;
    CompressedBuffer.resize(CompressedBufferSize);
    Success = Compress(Compressor, buf.data(), buf.size(), CompressedBuffer.data(),
                       CompressedBuffer.size(), &CompressedDataSize);
    CloseCompressor(Compressor);
    ofs.write(CompressedBuffer.data(), CompressedDataSize);
    ofs.close();

    cache.push_back(layer);

    return layer.output_hash;
}

void ModManager::GameFilesReady()
{
    sort(begin(mods), end(mods), [](const auto& l, const auto& r) {
        return stricmp(l.Name().c_str(), r.Name().c_str()) < 0;
    });

    patching_file_thread = std::thread([this]() {
        const auto cache_directory = ModManager::GetCacheDirectory();

        CollectPatchableFiles();
        ReadCache();

        for (auto&& modded_file : modded_patchable_files_) {
            auto&& [game_path, on_disk_files] = modded_file;

            auto game_file = ReadGameFile(game_path);
            if (game_file.empty()) {
                spdlog::error("Failed to get original game file {}", game_path.string());
                continue;
            }
            xmlDocPtr game_xml           = nullptr;
            game_file                    = "<MEOW_XML_SUCKS>" + game_file + "</MEOW_XML_SUCKS>";
            auto        game_file_hash   = GetDataHash(game_file);
            std::string last_valid_cache = "";
            std::string next_input_hash  = game_file_hash;

            for (auto&& on_disk_file : on_disk_files) {
                auto       patch_file_hash = GetFileHash(on_disk_file);
                const auto output_hash =
                    CheckCacheLayer(game_path, next_input_hash, patch_file_hash);
                if (output_hash) {
                    // Cache hit
                    last_valid_cache = *output_hash;
                    next_input_hash  = *output_hash;
                } else {
                    next_input_hash = "";

                    if (!game_xml) {
                        std::string cache_data = "";
                        if (last_valid_cache.empty()) {
                            cache_data = game_file;
                        } else {
                            cache_data = "<MEOW_XML_SUCKS>"
                                         + ReadCacheLayer(game_path, last_valid_cache)
                                         + "</MEOW_XML_SUCKS>";
                        }
                        game_xml = xmlReadMemory(cache_data.data(), cache_data.size(), "", "UTF-8",
                                                 XML_PARSE_RECOVER);
                    }

                    // Cache miss
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

                    if (last_valid_cache.empty()) {
                        last_valid_cache = game_file_hash;
                    }
                    last_valid_cache = PushCacheLayer(game_path, last_valid_cache, patch_file_hash,
                                                      buf, on_disk_file.string());
                    file_cache[game_path] = {buf.size(), true, buf};
                }
            }
            if (!game_xml) {
                auto cache_data       = ReadCacheLayer(game_path, last_valid_cache);
                file_cache[game_path] = {cache_data.size(), true, cache_data};
            }

            WriteCacheInfo(game_path);

            xmlFree(game_xml);
            game_xml = nullptr;
        }
    });
}

bool ModManager::IsFileModded(const fs::path& path) const
{
    for (const auto& mod : mods) {
        if (mod.HasFile(path.lexically_normal())) {
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

fs::path ModManager::GetCacheDirectory()
{
    return ModManager::GetModsDirectory() / ".cache";
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

std::string ModManager::ReadGameFile(fs::path path) const
{
    std::string output;

    char*  buffer           = nullptr;
    size_t output_data_size = 0;
    if (anno::ReadFileFromContainer(*(uintptr_t*)GetAddress(anno::SOME_GLOBAL_STRUCTURE_ARCHIVE),
                                    path.wstring().c_str(), &buffer, &output_data_size)) {
        buffer = buffer;
        output = {buffer, output_data_size};

        // TODO(alexander): Move to anno api
        static auto game_free = (decltype(free)*)(GetProcAddress(
            GetModuleHandleA("api-ms-win-crt-heap-l1-1-0.dll"), "free"));
        game_free(buffer);
    }
    return output;
}
