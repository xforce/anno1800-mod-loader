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
    using include_loader_t = std::function<XmlOperationContext(fs::path)>;

    XmlOperationContext();
    XmlOperationContext(const fs::path& mod_relative_path,
                        const fs::path& mod_base_path,
                        std::string_view mod_name = {});
    XmlOperationContext(const char* buffer, size_t size,
                        include_loader_t include_loader,
                        const fs::path& doc_path,
                        std::string_view mod_name = {});

    inline XmlOperationContext OpenInclude(fs::path file_path) const;
    
    inline size_t GetLine(pugi::xml_node node) const { return node.offset_debug(); }
    size_t GetLine(ptrdiff_t offset) const;
    
    pugi::xml_node GetRoot() const;
    inline const fs::path& GetPath() const { return doc_path_; }
    inline const std::string& GetName() const { return mod_name_; }

    template<typename... Args> void Debug(std::string_view msg, const Args &... args) const;
    void Warn(std::string_view msg, pugi::xml_node node = {}) const;
    void Error(std::string_view msg, pugi::xml_node node = {}) const;
    
private:
    std::string mod_name_;
    std::shared_ptr<pugi::xml_document> doc_;
    offset_data_t offset_data_;
    include_loader_t include_loader_;
    fs::path doc_path_;

    bool ReadFile(const fs::path& file_path, std::vector<char>& buffer, size_t& size);
    static offset_data_t BuildOffsetData(const char* buffer, size_t size);
};

class XmlOperation
{
public:
    enum Type { None, Add, AddNextSibling, AddPrevSibling, Remove, Replace, Merge, Group };

    XmlOperation(XmlOperationContext doc, pugi::xml_node node,
                 std::string guid = "", std::string temp = "", std::string mod_name = "",
                 fs::path game_path = {});

    Type                                            GetType() const;
    std::string                                     GetPath();

    void Apply(std::shared_ptr<pugi::xml_document> doc);

public:
    static std::vector<XmlOperation> GetXmlOperations(
        XmlOperationContext doc,
        fs::path    game_path,
        std::optional<pugi::xml_object_range<pugi::xml_node_iterator>> nodes = {});
    static std::vector<XmlOperation> GetXmlOperationsFromFile(
        fs::path    file_path,
        std::string mod_name,
        fs::path    game_path,
        fs::path    mod_path);

private:
    Type        type_;
    std::string path_;

    std::string speculative_path_;
    std::string guid_;
    std::string template_;

    std::optional<pugi::xml_object_range<pugi::xml_node_iterator>> nodes_;

    XmlOperationContext doc_;
    pugi::xml_node node_;

    std::string    condition_;
    bool           allow_no_match_ = false;
    std::string    content_;

    std::vector<XmlOperation> group_;

    std::string mod_name_;
    fs::path    game_path_;
    fs::path    mod_path_;

    enum SpeculativePathType {
        NONE,
        SINGLE_ASSET,
        ASSET_CONTAINER,
        SINGLE_TEMPLATE,
        TEMPLATE_CONTAINER,
    };

    SpeculativePathType speculative_path_type_ = SpeculativePathType::NONE;

    static std::string GetXmlPropString(pugi::xml_node node, std::string prop_name)
    {
        return node.attribute(prop_name.c_str()).as_string();
    }
    void RecursiveMerge(pugi::xml_node root_game_node, pugi::xml_node game_node,
                        pugi::xml_node patching_node);
    void ReadPath(pugi::xml_node node, std::string guid = "", std::string temp = "");
    void ReadType(pugi::xml_node node, std::string mod_name, fs::path game_path);

    std::optional<pugi::xml_node> FindAsset(std::string guid, pugi::xml_node node);
    std::optional<pugi::xml_node> FindTemplate(std::string temp, pugi::xml_node node);

    std::optional<pugi::xml_node> FindAsset(std::shared_ptr<pugi::xml_document> doc,
                                            std::string                         guid);
    std::optional<pugi::xml_node> FindTemplate(std::shared_ptr<pugi::xml_document> doc,
                                               std::string                         temp);

    pugi::xpath_node_set ReadGuidNodes(std::shared_ptr<pugi::xml_document> doc);
    pugi::xpath_node_set ReadTemplateNodes(std::shared_ptr<pugi::xml_document> doc);

    bool CheckCondition(std::shared_ptr<pugi::xml_document> doc);
};
