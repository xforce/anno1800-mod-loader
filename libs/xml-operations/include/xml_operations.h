#include "libxml/parser.h"
#include "libxml/xpath.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class XmlOperation
{
  public:
    enum Type { Add, AddNextSibling, AddPrevSibling, Remove, Replace, Merge };

    explicit XmlOperation(xmlNode *node);
    XmlOperation(xmlNode *node, std::string guid);
    void ReadPath();
    xmlNode *GetContentNode()
    {
        return node_;
    }

    Type GetType() const
    {
        return type_;
    }

    std::string GetPath()
    {
        return path_;
    }
    void                             Apply(xmlDocPtr doc);
    static std::vector<XmlOperation> GetXmlOperations(xmlNode *a_node);
    static std::vector<XmlOperation> GetXmlOperationsFromFile(fs::path path);

  private:
    Type        type_;
    std::string path_;
    std::string guid_;
    xmlNode *   node_ = nullptr;

    static inline std::string to_string(xmlChar *str)
    {
        int charLength = xmlStrlen(str);
        return std::string{reinterpret_cast<const char *>(str), size_t(charLength)};
    }

    static std::string GetXmlPropString(xmlNode *node, std::string prop_name)
    {
        auto prop   = xmlGetProp(node, (const xmlChar *)prop_name.c_str());
        auto result = to_string(prop);
        xmlFree(prop);
        return result;
    }
    void RecursiveMerge(xmlNode *game_node, xmlNode *patching_node);
    void ReadPath(xmlNode* node, std::string guid = "");
    void ReadType(xmlNode *node);
};
