
#include "libxml/parser.h"
#include "libxml/parserInternals.h"
#include "libxml/tree.h"
#include "libxml/xmlreader.h"
#include "libxml/xpath.h"

#include "xml_operations.h"

#include <cstdio>
#include <fstream>
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

    std::string buffer;
    xmlDocPtr   game_xml;
    buffer.resize(size);
    if (file.read(buffer.data(), size)) {
        buffer   = "<MEOW_XML_SUCKS>" + buffer + "</MEOW_XML_SUCKS>";
        game_xml = xmlReadMemory(buffer.data(), buffer.size(), "", NULL, XML_PARSE_RECOVER);
    }

    auto operations = XmlOperation::GetXmlOperationsFromFile(argv[2]);
    auto patch_xml  = xmlReadFile(argv[2], NULL, 0);

    for (auto &&operation : operations) {
        operation.Apply(game_xml);
    }

    xmlChar *xmlbuff;
    int      buffersize;
    xmlDocDumpFormatMemory(game_xml, &xmlbuff, &buffersize, 1);
    FILE *fp;
    fp = fopen("patched.xml", "w+");
    if (!fp) {
        printf("Could not open file for writing\n");

        return 0;
    }
    std::string buf = (const char *)(xmlbuff);
    buf             = buf.substr(buf.find("<MEOW_XML_SUCKS>") + strlen("<MEOW_XML_SUCKS>"));
    buf             = buf.substr(0, buf.find("</MEOW_XML_SUCKS>"));
    fwrite(buf.data(), 1, buf.size(), fp);
    fclose(fp);
    return 0;
}
