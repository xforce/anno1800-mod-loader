
#define CATCH_CONFIG_MAIN
#define CATCH_SINGLE_INCLUDE
#include "catch2/catch.hpp"

#include "libxml/parser.h"
#include "libxml/parserInternals.h"
#include "libxml/tree.h"
#include "libxml/xmlreader.h"
#include "libxml/xpath.h"

#include "xml_operations.h"

#include <vector>
#include <string_view>

#define TEST_RUNNER_START
class TestRunner
{
public:
    TestRunner(std::string_view input, std::string_view patch) {
        {
            patch_doc_ = xmlReadMemory(patch.data(), patch.size(), "", "UTF-8", XML_PARSE_RECOVER);
            auto root   = xmlDocGetRootElement(patch_doc_);
            xml_operations_ = XmlOperation::GetXmlOperations(root);
        }
        {
            input_doc_ = xmlReadMemory(input.data(), input.size(), "", "UTF-8", XML_PARSE_RECOVER);
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
        auto path_expression = xmlXPathCompile(reinterpret_cast<const xmlChar *>(("/MEOW_XML_SUCKS" + std::string(path.data())).c_str()));
        auto xpathCtx = xmlXPathNewContext(input_doc_);
        auto xpathObj = xmlXPathCompiledEval(path_expression, xpathCtx);
        bool exists = false;
        if (xpathObj && xpathObj->nodesetval) {
            exists = xpathObj->nodesetval->nodeNr > 0;
        }
        xmlFree(xpathObj);
        xmlFree(xpathCtx);
        xmlFree(path_expression);
        return exists;
    }


    std::string DumpXml() {
        xmlChar* xmlbuff;
        int      buffersize;
        xmlDocDumpFormatMemory(input_doc_, &xmlbuff, &buffersize, 1);
        std::string buf = (const char*)(xmlbuff);
        buf = buf.substr(buf.find("<MEOW_XML_SUCKS>") + strlen("<MEOW_XML_SUCKS>"));
        buf = buf.substr(0, buf.find("</MEOW_XML_SUCKS>"));
        xmlFree(xmlbuff);
        return buf;
    }

    ~TestRunner() {
        xmlFree(patch_doc_);
        xmlFree(input_doc_);
    }
private:
    std::vector<XmlOperation> xml_operations_;
    xmlDocPtr patch_doc_;
    xmlDocPtr input_doc_;
};

#include "tests2.inc.hpp"
