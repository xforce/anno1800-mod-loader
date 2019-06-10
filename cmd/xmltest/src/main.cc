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
        doc->load_buffer(buffer.data(), buffer.size());
    }

    // Flatten shit
    std::vector<pugi::xml_node> assets;
    auto                        nodes = doc->select_nodes("//Assets");
    for (pugi::xpath_node node : nodes) {
        for (pugi::xml_node asset : node.node().children()) {
            assets.push_back(asset);
        }
    }

    auto new_doc    = std::make_shared<pugi::xml_document>();
    auto groups     = new_doc->root().append_child("AssetList").append_child("Groups");
    auto assets_xml = groups.append_child("Group").append_child("Assets");
    for (pugi::xml_node asset : assets) {
        auto guid = asset.parent().parent().find_child(
            [](pugi::xml_node x) { return x.name() == std::string("GUID"); });
        if (!guid) {
            assets_xml.append_copy(asset);
        } else {
            // Check if we have already created this one
            std::string guid_str = guid.first_child().value();
            auto        group    = new_doc->select_nodes(
                ("/AssetList/Groups/Group[GUID='" + guid_str + "']/Assets").c_str());
            pugi::xml_node group_xml;
            if (std::begin(group) == std::end(group)) {
                // No group create new
                group_xml = groups.append_child("Group");
                group_xml.append_child("GUID")
                    .append_child(pugi::xml_node_type::node_pcdata)
                    .set_value(guid_str.c_str());
                group_xml = group_xml.append_child("Assets");
            } else {
                group_xml = std::begin(group)->node();
            }
            group_xml.append_copy(asset);
        }
    }

    auto operations = XmlOperation::GetXmlOperationsFromFile(argv[2]);
    for (auto &&operation : operations) {
        operation.Apply(new_doc);
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
    fwrite(buf.data(), 1, buf.size(), fp);
    fclose(fp);
    return 0;
}
