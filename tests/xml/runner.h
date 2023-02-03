#include "pugixml.hpp"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/spdlog.h"

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
    TestRunner(std::string_view mod_base_path, std::string_view input, std::string_view patch) {
        {
            auto sink = std::make_shared<spdlog::sinks::ostream_sink_st>(test_log_);
            auto test_logger = std::make_shared<spdlog::logger>("test_logger", sink);
            test_logger->set_pattern("[%l] %v");
            test_logger->set_level(spdlog::level::info);
            spdlog::set_default_logger(test_logger);
        }
        {
            xml_operations_ = XmlOperation::GetXmlOperationsFromFile(fs::absolute(patch), "", input, fs::absolute(mod_base_path));
        }
        {
            input_doc_ = std::make_shared<pugi::xml_document>();
            input_doc_->load_file(input.data());
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
        pugi::xpath_node_set results = input_doc_->select_nodes(path.data());
        bool exists = false;
        if (std::end(results) != std::begin(results)) {
            exists = true;
        }
        return exists;
    }

    std::string DumpXml() {
        std::stringstream ss;
        input_doc_->print(ss, "   ");
        std::string buf = ss.str();
        return buf;
    }

    bool HasIssues() {
        auto log_content = test_log_.str();
        
        if (log_content.find("[warning]") != std::string::npos || 
            log_content.find("[error]") != std::string::npos) {
            return true;
        }

        return false;
    }

    std::string DumpLog() {
        return test_log_.str();
    }

    ~TestRunner() {
        spdlog::drop("test_logger");
    }
private:
    std::vector<XmlOperation> xml_operations_;
    std::shared_ptr<pugi::xml_document> input_doc_ = nullptr;
    std::ostringstream test_log_;
};
