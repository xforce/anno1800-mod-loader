#include "xml_operations.h"

#include "absl/strings/str_cat.h"
#include "pugixml.hpp"
#include "spdlog/spdlog.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

int main(int argc, const char **argv)
{
    if (argc < 3) {
        // TODO(alexander): Print usage
        return -1;
    }

    spdlog::set_level(spdlog::level::debug);

    std::ifstream   file(argv[1], std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string                         buffer;
    std::shared_ptr<pugi::xml_document> doc = std::make_shared<pugi::xml_document>();
    buffer.resize(size);
    if (file.read(buffer.data(), size)) {
        doc->load_buffer(buffer.data(), buffer.size());
    }

    auto operations = XmlOperation::GetXmlOperationsFromFile(argv[2], "", {}, fs::absolute(fs::current_path()));
    for (auto &&operation : operations) {
        operation.Apply(doc);
    }

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
    return 0;
}
