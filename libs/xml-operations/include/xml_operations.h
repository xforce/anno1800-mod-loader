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
    XmlOperationContext(fs::path mod_relative_path,
                        fs::path mod_base_path,
                        std::string mod_name = {});
    XmlOperationContext(std::shared_ptr<pugi::xml_document> doc,
                        std::function<std::shared_ptr<pugi::xml_document>(fs::path)> include_loader,
                        fs::path mod_relative_path,
                        std::string mod_name = {});

    size_t GetLine(pugi::xml_node node);
    size_t GetLine(ptrdiff_t offset);
    static size_t GetLine(fs::path file_path, ptrdiff_t offset);

    XmlOperationContext OpenInclude(fs::path file_path);

  //private:
    std::shared_ptr<pugi::xml_document> doc_;
    fs::path mod_base_path_;
    fs::path mod_relative_path_;
    
  private:
    std::vector<ptrdiff_t> offset_data_;
    std::function<std::shared_ptr<pugi::xml_document>(fs::path)> include_loader_;
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
    static std::vector<XmlOperation> GetXmlOperations(XmlOperationContext doc,
                                                      std::string mod_name,
                                                      fs::path    game_path);
    static std::vector<XmlOperation> GetXmlOperationsFromNodes(XmlOperationContext doc,
                                                      pugi::xml_object_range<pugi::xml_node_iterator> nodes,
                                                      std::string mod_name,
                                                      fs::path    game_path);
    static std::vector<XmlOperation> GetXmlOperationsFromFile(fs::path    file_path,
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
