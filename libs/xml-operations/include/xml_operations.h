#pragma once

#include "pugixml.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class XmlOperation
{
  public:
    enum Type { None, Add, AddNextSibling, AddPrevSibling, Remove, Replace, Merge };

    XmlOperation(std::shared_ptr<pugi::xml_document> doc, pugi::xml_node node,
                 std::string guid = "", std::string temp = "", std::string mod_name = "",
                 fs::path game_path = {}, fs::path mod_path = {});

    pugi::xml_object_range<pugi::xml_node_iterator> GetContentNode();
    Type                                            GetType() const;
    std::string                                     GetPath();

    void Apply(std::shared_ptr<pugi::xml_document> doc);

  public:
    static std::vector<XmlOperation> GetXmlOperations(std::shared_ptr<pugi::xml_document> doc,
                                                      std::string mod_name  = "",
                                                      fs::path    game_path = {},
                                                      fs::path    mod_path  = {},
                                                      fs::path    doc_path  = {});
    static std::vector<XmlOperation> GetXmlOperationsFromFile(fs::path    path,
                                                              std::string mod_name  = "",
                                                              fs::path    game_path = {},
                                                              fs::path    mod_path  = {});

  private:
    Type        type_;
    std::string path_;

    std::string speculative_path_;
    std::string guid_;
    std::string template_;

    std::optional<pugi::xml_object_range<pugi::xml_node_iterator>> nodes_;

    std::shared_ptr<pugi::xml_document> doc_; // This is here to keep the node below alive
    pugi::xml_node node_;

    bool           skip_ = false;

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
    void ReadType(pugi::xml_node node, std::string mod_name, fs::path game_path, fs::path mod_path);

    std::optional<pugi::xml_node> FindAsset(std::string guid, pugi::xml_node node);
    std::optional<pugi::xml_node> FindTemplate(std::string temp, pugi::xml_node node);

    std::optional<pugi::xml_node> FindAsset(std::shared_ptr<pugi::xml_document> doc,
                                            std::string                         guid);
    std::optional<pugi::xml_node> FindTemplate(std::shared_ptr<pugi::xml_document> doc,
                                               std::string                         temp);

    pugi::xpath_node_set ReadGuidNodes(std::shared_ptr<pugi::xml_document> doc);
    pugi::xpath_node_set ReadTemplateNodes(std::shared_ptr<pugi::xml_document> doc);
};
