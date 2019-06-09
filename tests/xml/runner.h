#include "libxml/parser.h"
#include "libxml/parserInternals.h"
#include "libxml/tree.h"
#include "libxml/xmlreader.h"
#include "libxml/xpath.h"
#include "pugixml.hpp"

#include "xml_operations.h"

#include "catch2/catch.hpp"

#include <vector>
#include <string_view>
#include <cstring>
#include <sstream>
#include <memory>

class TestRunner
{
public:
    TestRunner(std::string_view input, std::string_view patch) {
        {
            patch_doc_ = std::make_shared<pugi::xml_document>();
            patch_doc_->load_buffer(patch.data(), patch.size());
            xml_operations_ = XmlOperation::GetXmlOperations(patch_doc_);
        }
        {
            input_doc_ = std::make_shared<pugi::xml_document>();
            input_doc_->load_buffer(input.data(), input.size());
        }
    }

    void ApplyPatches() {
        for (auto &&operation : xml_operations_) {
            operation.Apply(input_doc_);
        }
    }

    auto GetPatchedDoc() {
        return input_doc_;
    }

    bool PathExists(std::string_view path) {
        pugi::xpath_node_set results = input_doc_->select_nodes(("/MEOW_XML_SUCKS" + std::string(path.data())).c_str());
        bool exists = false;
        if (std::end(results) != std::begin(results)) {
            exists = true;
        }
        return exists;
    }


    std::string DumpXml() {
        std::stringstream ss;
        input_doc_->print(ss);
        std::string buf = ss.str();
        buf = buf.substr(buf.find("<MEOW_XML_SUCKS>") + strlen("<MEOW_XML_SUCKS>"));
        buf = buf.substr(0, buf.find("</MEOW_XML_SUCKS>"));
        return buf;
    }

    ~TestRunner() = default;
private:
    std::vector<XmlOperation> xml_operations_;
    std::shared_ptr<pugi::xml_document> patch_doc_ = nullptr;
    std::shared_ptr<pugi::xml_document> input_doc_ = nullptr;
};
