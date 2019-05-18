#include "xml_operations.h"

XmlOperation::XmlOperation(xmlNode *node)
{
    auto type = GetXmlPropString(node, "Type");
    if (type == "add") {
        type_ = Type::Add;
    } else if (type == "add") {
        type_ = Type::Add;
    } else if (type == "remove") {
        type_ = Type::Remove;
    } else if (type == "replace") {
        type_ = Type::Replace;
    } else if (type == "merge") {
        type_ = Type::Merge;
    }
    path_ = GetXmlPropString(node, "Path");
    if (path_ == "/") {
        path_ = "/*";
    }
    if (type_ != Type::Remove) {
        node_ = node->children;
        while (node_->type != XML_ELEMENT_NODE) {
            node_ = node_->next;
        }
    }
}

void MergeProperties(xmlNode *game_node, xmlNode *patching_node)
{
    xmlAttr *attribute = patching_node->properties;
    while (attribute) {
        xmlChar *value = xmlNodeListGetString(patching_node->doc, attribute->children, 1);
        // do something with value
        xmlSetProp(game_node, attribute->name, value);
        xmlFree(value);
        attribute = attribute->next;
    }
}

void RecursiveMerge(xmlNode *game_node, xmlNode *patching_node)
{
    if (!patching_node) {
        return;
    }
    xmlNode *cur_node = NULL;
    auto find_node_with_name = [](auto game_node, auto name) -> xmlNode* {
        xmlNode* cur_node = NULL;
        for (cur_node = game_node; cur_node; cur_node = cur_node->next) {
            if (xmlStrcmp(cur_node->name, name) == 0) {
                return cur_node;
            }
        }
        return nullptr;
    };

    for (cur_node = patching_node; cur_node; cur_node = cur_node->next) {
        game_node = find_node_with_name(game_node, cur_node->name);
        MergeProperties(game_node, cur_node);
        if (game_node) {
            if (game_node->type == XML_TEXT_NODE) {
                xmlNodeSetContent(game_node, cur_node->content);
            }
            else {
                RecursiveMerge(game_node->children, cur_node->children);
            }
        }
    }
}

void XmlOperation::Apply(xmlDocPtr doc)
{
    auto path_expression = xmlXPathCompile(
        reinterpret_cast<const xmlChar *>((std::string("/MEOW_XML_SUCKS") + GetPath()).c_str()));
    auto xpathCtx = xmlXPathNewContext(doc);
    auto xpathObj = xmlXPathCompiledEval(path_expression, xpathCtx);
    if (xpathObj->nodesetval) {
        for (int i = 0; i < xpathObj->nodesetval->nodeNr; i++) {
            auto game_node = xpathObj->nodesetval->nodeTab[i];
            auto root_node = game_node;
            if (GetType() == XmlOperation::Type::Merge) {
                auto patching_node = GetContentNode();
                RecursiveMerge(game_node, patching_node);
            } else if (GetType() == XmlOperation::Type::Add) {
                // TODO(alexander): Walking down next here adds nodes to unexpected places
                auto node = xmlDocCopyNodeList(game_node->doc, GetContentNode());
                while (game_node) {
                    if (game_node->type == XML_ELEMENT_NODE) {
                        xmlAddChildList(game_node, node);
                    }
                    game_node = nullptr;
                }
            } else if (GetType() == XmlOperation::Type::Remove) {
                while (game_node) {
                    xmlUnlinkNode(game_node);
                    game_node = game_node->next;
                }
            } else if (GetType() == XmlOperation::Type::Replace) {
                auto node = xmlDocCopyNodeList(game_node->doc, GetContentNode());
                auto parent = root_node->parent;
                while (game_node) {
                    auto next_game_node = game_node->next;
                    xmlUnlinkNode(game_node);
                    game_node = next_game_node;
                }
                xmlAddChildList(parent, node);
            }
        }
    }
}

std::vector<XmlOperation> XmlOperation::GetXmlOperations(xmlNode *a_node)
{
    std::vector<XmlOperation> mod_operations;
    if (reinterpret_cast<const char *>(a_node->name) == std::string("ModOps")) {
        xmlNode *cur_node      = nullptr;
        xmlNode *previous_node = nullptr;

        for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
            if (cur_node->type == XML_ELEMENT_NODE) {
                if (reinterpret_cast<const char *>(cur_node->name) == std::string("ModOp")) {
                    mod_operations.emplace_back(cur_node);
                }
            }
        }
    }
    return mod_operations;
}

std::vector<XmlOperation> XmlOperation::GetXmlOperationsFromFile(fs::path path)
{
    auto doc    = xmlReadFile(path.string().c_str(), "UTF-8", 0);
    auto root   = xmlDocGetRootElement(doc);
    auto result = GetXmlOperations(root);
    // xmlFree(doc);
    return result;
}
