#include "xml_operations.h"

#include "absl/strings/str_cat.h"
#include "pugixml.hpp"
#include "spdlog/spdlog.h"

#include "parseArguments.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

void apply_patch(std::shared_ptr<pugi::xml_document> doc, const fs::path& modPath)
{    
    const fs::path& patchPath = modPath / "data/config/export/main/asset/assets.xml";
    spdlog::debug("Prepatch: {}", patchPath.string());

    auto operations = XmlOperation::GetXmlOperationsFromFile(patchPath, 
        "xmltest", 
        patchPath.lexically_relative(modPath), 
        fs::absolute(modPath));
    for (auto& operation : operations) {
        operation.Apply(doc);
    }
}

int main(int argc, const char **argv)
{
    XmltestParameters params;
    if (!parseArguments(argc, argv, params)) {
        return -1;
    }

    std::string patch_content;
    if (params.useStdin) {
        for (std::string line; std::getline(std::cin, line); ) {
            patch_content += line + "\n";
        }
    }

    spdlog::set_level(params.verbose ? spdlog::level::debug : spdlog::level::info);
    spdlog::debug("Target: {}", params.targetPath.string());
    spdlog::debug("Patch: {}", params.patchPath.string());
    for (auto& path : params.modPaths) {
        spdlog::debug("Mod path: {}", path.string());
    }

    std::ifstream file(params.targetPath, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string buffer;
    std::shared_ptr<pugi::xml_document> doc = std::make_shared<pugi::xml_document>();
    buffer.resize(size);
    if (file.read(buffer.data(), size)) {
        doc->load_buffer(buffer.data(), buffer.size());
    }

    for (auto dep : params.prepatchPaths) {
        apply_patch(doc, dep);
    }

    const auto mod_path = fs::absolute(params.modPaths.front());
    const auto game_path = params.patchPath.lexically_relative(mod_path);
    const auto mod_name = "xmltest";

    auto loader = [&mod_path, &mod_name, &patch_content, &game_path, &params](const fs::path& file_path) {
        std::vector<char> buffer;
        size_t size;
        spdlog::debug("Include: {}", file_path.string());

        // handle additional paths
        fs::path search_path = mod_path;
        for (auto& path : params.modPaths) {
            if (fs::exists(path / file_path)) {
                search_path = path;
                break;
            }
        }

        if (file_path == params.stdinPath) {
            return std::make_shared<XmlOperationContext>(patch_content.data(), patch_content.size(), file_path, mod_name);
        }

        // read found (or just mod_path)
        if (!XmlOperationContext::ReadFile(search_path / file_path, buffer, size)) {
            spdlog::error("{}: Failed to open {}", mod_name, file_path.string());
            return std::make_shared<XmlOperationContext>();
        }
        return std::make_shared<XmlOperationContext>(buffer.data(), size, file_path, mod_name);
    };
    auto context = game_path == params.stdinPath ?
        std::make_shared<XmlOperationContext>(patch_content.data(), patch_content.size(), game_path, mod_name, loader) :
        std::make_shared<XmlOperationContext>(game_path, mod_path, mod_name);
    context->SetLoader(loader);

    auto operations = XmlOperation::GetXmlOperations(context, game_path);
    for (auto& operation : operations) {
        operation.Apply(doc);
    }

    if (!params.skipOutput) {
        struct xml_string_writer : pugi::xml_writer {
            std::string result;

            virtual void write(const void *data, size_t size)
            {
                absl::StrAppend(&result, std::string_view{(const char *)data, size});
            }
        };

        xml_string_writer writer;

        spdlog::info("Start writing");
        writer.result.reserve(100 * 1024 * 1024);
        doc->print(writer);
        spdlog::info("Finished writing");
        
        FILE *fp;
        fp = fopen("patched.xml", "w+");
        if (!fp) {
            printf("Could not open file for writing\n");
            return 0;
        }
        fwrite(writer.result.data(), 1, writer.result.size(), fp);
        fclose(fp);
    }
    return 0;
}
