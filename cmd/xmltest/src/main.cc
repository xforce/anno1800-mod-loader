#include "xml_operations.h"

#include "pugixml.hpp"

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

    std::ifstream   file(argv[1], std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string                         buffer;
    std::shared_ptr<pugi::xml_document> doc = std::make_shared<pugi::xml_document>();
    buffer.resize(size);
    if (file.read(buffer.data(), size)) {
        buffer = "<MEOW_XML_SUCKS>" + buffer + "</MEOW_XML_SUCKS>";
        doc->load_buffer(buffer.data(), buffer.size());
    }

    // Flatten shit
    // Collect all groups in structure that makes sense

    auto operations = XmlOperation::GetXmlOperationsFromFile(argv[2]);
    for (auto &&operation : operations) {
        operation.Apply(doc);
    }

    std::stringstream ss;
    doc->print(ss);
    FILE *fp;
    fp = fopen("patched.xml", "w+");
    if (!fp) {
        printf("Could not open file for writing\n");

        return 0;
    }
    std::string buf = ss.str();
    buf             = buf.substr(buf.find("<MEOW_XML_SUCKS>") + strlen("<MEOW_XML_SUCKS>"));
    buf             = buf.substr(0, buf.find("</MEOW_XML_SUCKS>"));
    fwrite(buf.data(), 1, buf.size(), fp);
    fclose(fp);
    return 0;
}
