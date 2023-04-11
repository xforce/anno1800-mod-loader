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
    using include_loader_t = std::function<std::shared_ptr<XmlOperationContext>(const fs::path&)>;

    XmlOperationContext();
    XmlOperationContext(const fs::path& mod_relative_path,
                        const fs::path& mod_base_path,
                        std::string     mod_name = {});
    XmlOperationContext(const char* buffer, size_t size,
                        const fs::path& doc_path,
                        const std::string& mod_name = {},
                        std::optional<include_loader_t> include_loader = {});

    std::shared_ptr<XmlOperationContext> OpenInclude(const fs::path& file_path) const;

    void SetLoader(include_loader_t loader) { include_loader_ = loader; }

    size_t GetLine(pugi::xml_node node) const { return GetLine(node.offset_debug()); }
    size_t GetLine(ptrdiff_t offset) const;

    std::shared_ptr<pugi::xml_document> GetDoc() const { return doc_; }
    pugi::xml_node GetRoot() const;
    fs::path GetPath() const { return doc_path_; }
    const std::string& GetGenericPath() const { return doc_path_; }
    const std::string& GetName() const { return mod_name_; }

    template<typename... Args> void Debug(std::string_view msg, const Args &... args) const;
    void Debug(std::string_view msg, pugi::xml_node node) const;
    void Warn(std::string_view msg, pugi::xml_node node = {}) const;
    void Error(std::string_view msg, pugi::xml_node node = {}) const;

    static bool ReadFile(const fs::path& file_path, std::vector<char>& buffer, size_t& size);

private:
    std::string mod_name_;
    std::shared_ptr<pugi::xml_document> doc_;
    offset_data_t offset_data_;
    std::optional<include_loader_t> include_loader_;
    std::string doc_path_;

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
              std::shared_ptr<XmlOperationContext> context,
              pugi::xml_node node);

    /// @brief Select XPath nodes.
    /// @param assetNode Start search here. Resulting asset is stored back.
    /// @param strict Skip normal XPath selection if GUID or Template is specified.
    pugi::xpath_node_set Select(std::shared_ptr<pugi::xml_document> doc,
        std::optional<pugi::xml_node>* assetNode = nullptr,
        bool strict = false) const;

    bool IsEmpty() const { return empty_path_; };
    bool IsNegative() const { return negative_; };
    const std::string& GetPath() const { return path_; };

private:
    std::shared_ptr<XmlOperationContext> context_;
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
    std::optional<pugi::xml_node> FindAsset(const std::string& guid, pugi::xml_node node, int speculate_position = 2) const;
    std::optional<pugi::xml_node> FindTemplate(const std::string& temp, pugi::xml_node node) const;
    std::optional<pugi::xml_node> FindTemplate(std::shared_ptr<pugi::xml_document> doc, const std::string& templ) const;

    /// @brief Select XPath nodes via Values/Standard/GUID.
    /// @param assetNode Start search here. Resulting asset is stored back.
    pugi::xpath_node_set ReadGuidNodes(std::shared_ptr<pugi::xml_document> doc, 
        std::optional<pugi::xml_node>* assetNode) const;
    pugi::xpath_node_set ReadTemplateNodes(std::shared_ptr<pugi::xml_document> doc) const;
};

class XmlOperation
{
public:
    enum Type { None, Add, AddNextSibling, AddPrevSibling, Remove, Replace, Merge, Group };

    XmlOperation(std::shared_ptr<XmlOperationContext> doc, pugi::xml_node node,
                 const std::string& guid = "", const std::string& templ = "");

    Type GetType() const;

    void Apply(std::shared_ptr<pugi::xml_document> doc);

public:
    static std::vector<XmlOperation> GetXmlOperations(
        std::shared_ptr<XmlOperationContext> doc,
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

    std::shared_ptr<XmlOperationContext> doc_;
    pugi::xml_node node_;

    std::vector<XmlOperation> group_;

    static std::string GetXmlPropString(pugi::xml_node node, const std::string& prop_name)
    {
        return node.attribute(prop_name.c_str()).as_string();
    }
    void RecursiveMerge(pugi::xml_node root_game_node, pugi::xml_node game_node,
                        pugi::xml_node patching_node);
    void ReadType(pugi::xml_node node);

    /// @brief Check Condition XPath. Can use GUID attribute.
    //         True when nodes are found.
    //         Can be negated with `!`.
    /// @param assetNode Returns GUID asset if found.
    bool CheckCondition(std::shared_ptr<pugi::xml_document> doc, std::optional<pugi::xml_node>& assetNode);
};
