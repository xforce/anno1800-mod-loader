#include "mod_manager.h"

#include "meow_hash_x64_aesni.h"

#include "anno/random_game_functions.h"
#include "xml_operations.h"

#include "absl/strings/str_cat.h"
#include "spdlog/spdlog.h"

#define ZSTD_STATIC_LINKING_ONLY /* ZSTD_compressContinue, ZSTD_compressBlock */
#include "fse.h"
#include "zstd.h"
#include "zstd_errors.h" /* ZSTD_getErrorCode */

// Prevent preprocess errors with boringssl
#undef X509_NAME
#undef X509_CERT_PAIR
#undef X509_EXTENSIONS
#include "openssl/sha.h"

#include <Windows.h>

#include <fstream>
#include <optional>
#include <sstream>

constexpr static auto PATCH_OP_VERSION = "1.17";

Mod& ModManager::Create(const fs::path& root)
{
    spdlog::info("Loading mod {}", root.stem().string());
    auto& mod = this->mods_.emplace_back(root);
    return mod;
}

static bool IsModEnabled(fs::path path)
{
    // If mod folder name starts with '-', we don't enable it.
    return path.stem().wstring().find(L'-') != 0;
}

void ModManager::LoadMods()
{
    this->mods_.clear();
    this->file_cache_.clear();
    this->modded_patchable_files_.clear();

    auto mods_directory = ModManager::GetModsDirectory();
    if (mods_directory.empty()) {
        return;
    }

    // We have a mods directory
    // Now create a mod for each of these
    std::vector<fs::path> mod_roots;
    for (auto&& root : fs::directory_iterator(mods_directory)) {
        if (root.is_directory()) {
            if (IsModEnabled(root.path())) {
                this->Create(root.path());
            } else {
                spdlog::info("Disabled mod {}", root.path().stem().string());
            }
        }
    }
    if (this->mods_.empty()) {
        spdlog::info("No mods found in {}", mods_directory.string());
    }
}

const std::vector<std::string>& ModManager::GetPythonScripts() const
{
    return python_scripts_;
}

void ModManager::CollectPatchableFiles()
{
    for (const auto& mod : mods_) {
        mod.ForEachFile([this](const fs::path& game_path, const fs::path& file_path) {
            if (IsPatchableFile(game_path)) {
                modded_patchable_files_[game_path].emplace_back(file_path);
            } else {
                if (IsPythonStartScript(file_path)) {
                    auto        mods_directory = ModManager::GetModsDirectory();
                    std::string start_script   = "console.startScript('mods\\"
                                               + fs::relative(file_path, mods_directory).string()
                                               + "')";
                    spdlog::info("Loading ptyhon script {}", start_script);
                    python_scripts_.emplace_back(start_script);
                } else {
                    auto hFile = CreateFileW(file_path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        LARGE_INTEGER lFileSize;
                        GetFileSizeEx(hFile, &lFileSize);
                        CloseHandle(hFile);
                        file_cache_[game_path] = {
                            static_cast<size_t>(lFileSize.QuadPart), false, {}, file_path};
                    }
                }
            }
        });
    }
}

void ModManager::StartWatchingFiles()
{
    //
    if (this->watch_file_thread_.joinable()) {
        return;
    }
    this->watch_file_thread_ = std::thread([this]() {
        const auto cache_directory = ModManager::GetModsDirectory();
        if (cache_directory.empty()) {
            return;
        }

        HANDLE hDir = CreateFileW(cache_directory.wstring().c_str(), FILE_LIST_DIRECTORY,
                                  FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                                  OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        watch_file_ov_.OffsetHigh = 0;
        watch_file_ov_.hEvent     = CreateEvent(NULL, TRUE, FALSE, NULL);

        while (TRUE) {
            DWORD                   BytesReturned;
            DWORD                   bytesRet = 0;
            FILE_NOTIFY_INFORMATION Buffer[1024];
            memset(Buffer, 0, sizeof(Buffer));

            ReadDirectoryChangesW(hDir, Buffer, sizeof(Buffer), true,
                                  FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME
                                      | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE
                                      | FILE_NOTIFY_CHANGE_CREATION,
                                  &bytesRet, &watch_file_ov_, NULL);
            WaitForSingleObject(watch_file_ov_.hEvent, INFINITE);
            if (shuttding_down_.load()) {
                return;
            }
            bool  need_recreate_mods = false;
            BYTE* pBase              = (BYTE*)Buffer;

            for (;;) {
                FILE_NOTIFY_INFORMATION& info = (FILE_NOTIFY_INFORMATION&)*pBase;
                std::wstring             filename(info.FileName);
                if (filename.find(L".cache") == 0
                    || filename.find(L"start_script.py") != std::wstring::npos) {
                    if (!info.NextEntryOffset) {
                        break;
                    }
                    continue;
                }
                /* switch (info->Action) {
                     case FILE_ACTION_ADDED:
                         break;
                     case FILE_ACTION_MODIFIED:
                         break;
                     case FILE_ACTION_REMOVED:
                         break;
                     case FILE_ACTION_RENAMED_NEW_NAME:
                         break;
                     case FILE_ACTION_RENAMED_OLD_NAME:
                         break;
                 }*/
                need_recreate_mods = true;
                if (!info.NextEntryOffset) {
                    break;
                }
                pBase += info.NextEntryOffset;
            }

            if (need_recreate_mods) {
                if (!this->reload_mods_thread_.joinable()) {
                    this->reload_mods_thread_ = std::thread([this]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        {
                            std::lock_guard<std::mutex> lk(mods_ready_mx_);
                            mods_ready_.store(false);
                        }

                        spdlog::info("Reloading mods");
                        LoadMods();
                        GameFilesReady();
                        spdlog::info("Waiting for mods to finish");
                        WaitModsReady();
                        spdlog::info("Triggering game reload");
                        anno::ToolOneDataHelper::ReloadData();
                        auto tool_one_helper =
                            *(uint64_t*)GetAddress(anno::SOME_GLOBAL_STRUCT_TOOL_ONE_HELPER_MAYBE);
                        auto magic_wait_time = *(uint64_t*)(tool_one_helper + 0x160);
                        magic_wait_time      = magic_wait_time;
                        *(uint64_t*)(tool_one_helper + 0x160) -=
                            3000; // Remove stupid 5 second wait for reload
                        // magic_wait_time
                        this->reload_mods_thread_.detach();
                        this->reload_mods_thread_ = {};
                    });
                } else {
                    mods_change_wile_reload_.store(true);
                }
            }
        }
    });
}

void ModManager::WaitModsReady() const
{
    //
    std::unique_lock<std::mutex> lk(this->mods_ready_mx_);
    this->mods_ready_cv_.wait(lk, [this] { return this->mods_ready_.load(); });
}

Mod& ModManager::GetModContainingFile(const fs::path& file)
{
    for (auto& mod : mods_) {
        if (file.lexically_normal().generic_string().find(mod.Path().lexically_normal().generic_string()) == 0) {
            return mod;
        }
    }
    static Mod null_mod;
    assert(false);
    return null_mod;
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
                    spdlog::debug("Loading from cache {} vs {}",
                                  patch_op_version, PATCH_OP_VERSION);
                    std::vector<CacheLayer> cache_layers;
                    for (auto&& layer : layer_order) {
                        cache_layers.emplace_back(data["layers"].at(layer).get<CacheLayer>());
                    }
                    modded_file_cache_info_[game_path] = std::move(cache_layers);
                } else {
                    spdlog::debug("Skipping cache because Patch Op Version mismatch {} vs {}",
                                  patch_op_version, PATCH_OP_VERSION);
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
                std::string output;
                size_t      rSize = ZSTD_getFrameContentSize(buffer.data(), buffer.size());
                output.resize(rSize);
                size_t dSize =
                    ZSTD_decompress(output.data(), output.size(), buffer.data(), buffer.size());
                output.resize(dSize);
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
    spdlog::debug("PushCacheLayer {} {} {} {}", game_path.string(), last_valid_cache, patch_file_hash,
                  mod_name);
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

    size_t const cBuffSize = ZSTD_compressBound(buf.size());
    std::string  CompressedBuffer;
    CompressedBuffer.resize(cBuffSize);
    size_t const cSize =
        ZSTD_compress(CompressedBuffer.data(), CompressedBuffer.size(), buf.data(), buf.size(), 1);
    CompressedBuffer.resize(cSize);

    ofs.write(CompressedBuffer.data(), CompressedBuffer.size());
    ofs.close();

    cache.push_back(layer);

    return layer.output_hash;
}

void ModManager::EnsureDummy()
{
    static auto dummy_path = ModManager::GetDummyPath();
    if (!fs::exists(dummy_path)) {
        fs::create_directories(ModManager::GetCacheDirectory());
        std::fstream fs;
        fs.open(dummy_path, std::ios::out);
        fs.close();
    }
}

void ModManager::GameFilesReady()
{
    if (this->mods_ready_.load() || patching_file_thread_.joinable()) {
        spdlog::debug("Skip patching, we are either doing it already or are done");
        return;
    }

    sort(begin(mods_), end(mods_), [](const auto& l, const auto& r) {
        return stricmp(l.Name().c_str(), r.Name().c_str()) < 0;
    });

    ModManager::EnsureDummy();

    patching_file_thread_ = std::thread([this]() {
        spdlog::info("Start applying xml operations");

        const auto cache_directory = ModManager::GetCacheDirectory();

        CollectPatchableFiles();
        ReadCache();

        for (auto&& modded_file : modded_patchable_files_) {
            if (shuttding_down_.load()) {
                return;
            }

            auto&& [game_path, on_disk_files] = modded_file;

            auto game_file = ReadGameFile(game_path);
            if (game_file.empty()) {
                for (auto& on_disk_file : on_disk_files) {
                    spdlog::error("Failed to get original game file {} {}", game_path.string(),
                                  on_disk_file.string());
                }
                continue;
            }
            std::shared_ptr<pugi::xml_document> game_xml         = nullptr;
            auto                                game_file_hash   = GetDataHash(game_file);
            std::string                         last_valid_cache = "";
            std::string                         next_input_hash  = game_file_hash;

            for (auto&& on_disk_file : on_disk_files) {
                if (shuttding_down_.load()) {
                    return;
                }
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
                            cache_data = ReadCacheLayer(game_path, last_valid_cache);
                        }
                        game_xml = std::make_shared<pugi::xml_document>();
                        auto parse_result =
                            game_xml->load_buffer(cache_data.data(), cache_data.size());
                        if (!parse_result) {
                            spdlog::error("Failed to parse cache {}: {}", on_disk_file.string(),
                                          parse_result.description());
                        }
                    }

                    // Cache miss
                    auto &mod = GetModContainingFile(on_disk_file);
                    auto operations = XmlOperation::GetXmlOperationsFromFile(
                        on_disk_file, mod.Name(), game_path, on_disk_file);
                    for (auto&& operation : operations) {
                        operation.Apply(game_xml);
                    }

                    struct xml_string_writer : pugi::xml_writer {
                        std::string result;

                        virtual void write(const void* data, size_t size)
                        {
                            absl::StrAppend(&result, std::string_view{(const char*)data, size});
                        }
                    };

                    xml_string_writer writer;
                    writer.result.reserve(100 * 1024 * 1024);
                    game_xml->print(writer);
                    std::string& buf = writer.result;

                    if (last_valid_cache.empty()) {
                        last_valid_cache = game_file_hash;
                    }
                    last_valid_cache = PushCacheLayer(game_path, last_valid_cache, patch_file_hash,
                                                      buf, on_disk_file.string());
                    file_cache_[game_path] = {buf.size(), true, buf};
                }
            }
            if (!game_xml) {
                auto cache_data        = ReadCacheLayer(game_path, last_valid_cache);
                file_cache_[game_path] = {cache_data.size(), true, cache_data};
            }

            WriteCacheInfo(game_path);

            game_xml = nullptr;
        }

        StartWatchingFiles();

        {
            std::lock_guard<std::mutex> lk(mods_ready_mx_);
            mods_ready_.store(true);
        }
        spdlog::info("Finished applying xml operations");

        mods_ready_cv_.notify_all();

        patching_file_thread_.detach();
        patching_file_thread_ = {};
    });
}

bool ModManager::IsFileModded(const fs::path& path) const
{
    for (const auto& mod : mods_) {
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
    WaitModsReady();
    {
        std::scoped_lock lk{file_cache_mutex_};
        if (file_cache_.count(path) > 0) {
            return file_cache_.at(path);
        }
    }
    // File not in cache, yet?
    throw std::logic_error("GetModdedFileInfo shouldn't be called on a file that is not modded");
}

ModManager::~ModManager()
{
    Shutdown();
}

void ModManager::Shutdown()
{
    shuttding_down_.store(true);
    //
    // Trigger watch abort
    if (watch_file_thread_.joinable()) {
        std::thread thread = {};
        std::swap(thread, watch_file_thread_);
        SetEvent(watch_file_ov_.hEvent);
        thread.join();
        thread = {};
    }
    if (patching_file_thread_.joinable()) {
        std::thread thread = {};
        std::swap(thread, patching_file_thread_);
        thread.join();
        thread = {};
    }
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
            auto mods_parent = fs::canonical(dll_file.parent_path() / ".." / "..");
            mods_directory   = mods_parent / "mods";
            if (!fs::exists(mods_directory)) {
                fs::create_directories(mods_directory);
            }
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

fs::path ModManager::GetDummyPath()
{
    return ModManager::GetCacheDirectory() / ".dummy";
}

bool ModManager::IsPythonStartScript(const fs::path& file) const
{
    const auto filename = file.filename();
    return filename == "start_script.py";
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
    int regs[4];
    __cpuid(regs, 1);
    int have_aes_ni = (regs[2] >> 25) & 1;

    if (have_aes_ni) {
        meow_u128   Hash = MeowHash(MeowDefaultSeed, data.size(), (char*)data.data());
        std::string result;
        for (auto& n : Hash.m128i_i8) {
            absl::StrAppend(&result, absl::Hex(n, absl::kZeroPad2));
        }
        return result;
    } else {
        uint8_t digest[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const uint8_t*>(data.data()), data.size(), digest);
        std::string result;
        for (auto& n : digest) {
            absl::StrAppend(&result, absl::Hex(n, absl::kZeroPad2));
        }
        return result;
    }
}

std::string ModManager::ReadGameFile(fs::path path)
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

fs::path ModManager::MapAliasedPath(fs::path path)
{
    if (path == L"data/config/game/asset/assets.xml") {
        return L"data/config/export/main/asset/assets.xml";
    }
    if (path == L"data/config/game/asset/properties.xml") {
        return L"data/config/export/main/asset/properties.xml";
    }
    if (path == L"data/config/game/asset/templates.xml") {
        return L"data/config/export/main/asset/templates.xml";
    }
    return path;
}
