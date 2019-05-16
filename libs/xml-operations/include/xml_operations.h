#include "libxml/parser.h"
#include "libxml/xpath.h"

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class XmlOperation
{
  public:
    enum Type { Add, Remove, Replace, Merge };

    explicit XmlOperation(xmlNode *node);

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
    xmlNode *   node_ = nullptr;

    inline std::string to_string(xmlChar *str)
    {
        int charLength = xmlStrlen(str);
        return std::string{reinterpret_cast<const char *>(str), size_t(charLength)};
    }

    std::string GetXmlPropString(xmlNode *node, std::string prop_name)
    {
        auto prop   = xmlGetProp(node, (const xmlChar *)prop_name.c_str());
        auto result = to_string(prop);
        xmlFree(prop);
        return result;
    }
};
