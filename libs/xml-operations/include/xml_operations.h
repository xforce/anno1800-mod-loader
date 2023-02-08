#pragma once

#include "pugixml.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class XmlOperationContext
{
public:
    using offset_data_t = std::vector<ptrdiff_t>;
    using include_loader_t = std::function<XmlOperationContext(const fs::path&)>;

    XmlOperationContext();
    XmlOperationContext(const fs::path& mod_relative_path,
                        const fs::path& mod_base_path,
                        std::string     mod_name = {});
    XmlOperationContext(const char* buffer, size_t size,
                        const fs::path& doc_path,
                        const std::string& mod_name = {},
                        std::optional<include_loader_t> include_loader = {});

    XmlOperationContext OpenInclude(const fs::path& file_path) const;

    void SetLoader(include_loader_t loader) { include_loader_ = loader; }

    size_t GetLine(pugi::xml_node node) const { return GetLine(node.offset_debug()); }
    size_t GetLine(ptrdiff_t offset) const;

    std::shared_ptr<pugi::xml_document> GetDoc() const { return doc_; }
    pugi::xml_node GetRoot() const;
    const fs::path& GetPath() const { return doc_path_; }
    const std::string& GetName() const { return mod_name_; }

    template<typename... Args> void Debug(std::string_view msg, const Args &... args) const;
    void Warn(std::string_view msg, pugi::xml_node node = {}) const;
    void Error(std::string_view msg, pugi::xml_node node = {}) const;

private:
    std::string mod_name_;
    std::shared_ptr<pugi::xml_document> doc_;
    offset_data_t offset_data_;
    std::optional<include_loader_t> include_loader_;
    fs::path doc_path_;

    bool ReadFile(const fs::path& file_path, std::vector<char>& buffer, size_t& size);
    static offset_data_t BuildOffsetData(const char* buffer, size_t size);
};

class XmlLookup
{
public:
    XmlLookup();
    XmlLookup(const std::string& path,
              const std::string& guid,
              const std::string& templ,
              bool explicit_speculative,
              const XmlOperationContext& context,
              pugi::xml_node node);

    pugi::xpath_node_set Select(std::shared_ptr<pugi::xml_document> doc) const;

    bool IsEmpty() const { return empty_path_; };
    bool IsNegative() const { return negative_; };
    const std::string& GetPath() const { return path_; };

private:
    XmlOperationContext context_;
    pugi::xml_node node_;

    bool empty_path_;
    bool negative_;
    std::string path_;
    std::string guid_;
    std::string template_;

    enum SpeculativePathType {
        NONE,
        SINGLE_ASSET,
        VALUES_CONTAINER,
        ASSET_CONTAINER,
        SINGLE_TEMPLATE,
        TEMPLATE_CONTAINER,
    };

    std::string speculative_path_;
    SpeculativePathType speculative_path_type_ = SpeculativePathType::NONE;

    void ReadPath(std::string prop_path, std::string guid, std::string templ);
    std::optional<pugi::xml_node> FindAsset(std::shared_ptr<pugi::xml_document> doc, const std::string& guid) const;
    std::optional<pugi::xml_node> FindAsset(const std::string& guid, pugi::xml_node node) const;
    std::optional<pugi::xml_node> FindTemplate(const std::string& temp, pugi::xml_node node) const;
    std::optional<pugi::xml_node> FindTemplate(std::shared_ptr<pugi::xml_document> doc, const std::string& templ) const;
    pugi::xpath_node_set ReadGuidNodes(std::shared_ptr<pugi::xml_document> doc) const;
    pugi::xpath_node_set ReadTemplateNodes(std::shared_ptr<pugi::xml_document> doc) const;
};

class XmlOperation
{
public:
    enum Type { None, Add, AddNextSibling, AddPrevSibling, Remove, Replace, Merge, Group };

    XmlOperation(XmlOperationContext doc, pugi::xml_node node,
                 const std::string& guid = "", const std::string& temp = "", const std::string& mod_name = "",
                 const fs::path& game_path = {});

    Type GetType() const;

    void Apply(std::shared_ptr<pugi::xml_document> doc);

public:
    static std::vector<XmlOperation> GetXmlOperations(
        XmlOperationContext doc,
        const fs::path&     game_path,
        std::optional<pugi::xml_object_range<pugi::xml_node_iterator>> nodes = {});
    static std::vector<XmlOperation> GetXmlOperationsFromFile(
        const fs::path&     file_path,
        std::string         mod_name,
        const fs::path&     game_path,
        const fs::path&     mod_path);

private:
    Type        type_;
    XmlLookup   path_;
    bool        allow_no_match_ = false;
    XmlLookup   condition_;
    XmlLookup   content_;

    std::optional<pugi::xml_object_range<pugi::xml_node_iterator>> nodes_;

    XmlOperationContext doc_;
    pugi::xml_node node_;

    std::vector<XmlOperation> group_;

    std::string mod_name_;
    fs::path    game_path_;
    fs::path    mod_path_;

    static std::string GetXmlPropString(pugi::xml_node node, const std::string& prop_name)
    {
        return node.attribute(prop_name.c_str()).as_string();
    }
    void RecursiveMerge(pugi::xml_node root_game_node, pugi::xml_node game_node,
                        pugi::xml_node patching_node);
    void ReadType(pugi::xml_node node);

    bool CheckCondition(std::shared_ptr<pugi::xml_document> doc);
};
