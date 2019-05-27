#include "xml_operations.h"

#include "absl/strings/str_split.h"
#include "spdlog/spdlog.h"

#include <cstdio>

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
    if (path_.length() > 0) {
        if (path_[path_.length() - 1] == '/') {
            path_ = path_.substr(0, path_.length() - 1);
        }
    }
    if (type_ != Type::Remove) {
        node_ = node->children;
    }
}

XmlOperation::XmlOperation(xmlNode *node, std::string guid)
{
    if (!guid.empty()) {
        path_ = "//Asset[Values/Standard/GUID='" + guid + "']";
    }
    auto prop_path = GetXmlPropString(node, "Path");
    if (prop_path.find("/") != 0) {
        path_ += "/";
    }
    path_ += GetXmlPropString(node, "Path");
    if (path_ == "/") {
        path_ = "/*";
    }
    if (path_.length() > 0) {
        if (path_[path_.length() - 1] == '/') {
            path_ = path_.substr(0, path_.length() - 1);
        }
    }
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
    if (type_ != Type::Remove) {
        node_ = node->children;
        while (node_->type != XML_ELEMENT_NODE) {
            node_ = node_->next;
        }
    }
}

void XmlOperation::Apply(xmlDocPtr doc)
{
    auto path_expression = xmlXPathCompile(
        reinterpret_cast<const xmlChar *>((std::string("/MEOW_XML_SUCKS") + GetPath()).c_str()));
    if (!path_expression) {
        spdlog::error("Failed to compile Path {}", GetPath());
        return;
    }
    auto xpathCtx = xmlXPathNewContext(doc);
    if (!xpathCtx) {
        spdlog::error("Failed to create XPath context");
        return;
    }
    auto xpathObj = xmlXPathCompiledEval(path_expression, xpathCtx);
    if (!xpathObj) {
        spdlog::error("Failed to evaluate Path {}", GetPath());
        return;
    }
    if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
        for (int i = 0; i < xpathObj->nodesetval->nodeNr; i++) {
            auto game_node = xpathObj->nodesetval->nodeTab[i];
            if (GetType() == XmlOperation::Type::Merge) {
                auto patching_node = GetContentNode();
                RecursiveMerge(game_node, patching_node);
            } else if (GetType() == XmlOperation::Type::Add) {
                // TODO(alexander): Walking down next here adds nodes to unexpected places
                auto node = xmlDocCopyNodeList(game_node->doc, GetContentNode());
                if (game_node->type == XML_ELEMENT_NODE) {
                    xmlAddChildList(game_node, node);
                }
                game_node = nullptr;
            } else if (GetType() == XmlOperation::Type::Remove) {
                xmlUnlinkNode(game_node);
                game_node = game_node->next;
            } else if (GetType() == XmlOperation::Type::Replace) {
                auto node = xmlDocCopyNodeList(game_node->doc, GetContentNode());
                auto node_to_add = node;
                while (node_to_add) {
                    xmlAddPrevSibling(game_node, xmlDocCopyNode(node_to_add, game_node->doc, 1));
                    node_to_add = node_to_add->next;
                }
                xmlUnlinkNode(game_node);
            }
        }
    } else {
        // TODO(alexander): Diagnostics
        spdlog::warn("No matching node for Path {}", GetPath());
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
                    const auto guid = GetXmlPropString(cur_node, "GUID");
                    if (!guid.empty()) {
                        std::vector<std::string> guids = absl::StrSplit(guid, ',');
                        for (auto g : guids) {
                            mod_operations.emplace_back(cur_node, g.data());
                        }
                    } else {
                        mod_operations.emplace_back(cur_node);
                    }
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

void MergeProperties(xmlNode* game_node, xmlNode* patching_node)
{
    xmlAttr* attribute = patching_node->properties;
    // xmlCopyPropList(game_node, attribute);
    while (attribute) {
        xmlChar* value = xmlNodeListGetString(patching_node->doc, attribute->children, 1);
        xmlSetProp(game_node, attribute->name, value);
        xmlFree(value);
        attribute = attribute->next;
    }
}

static bool HasNonTextNode(xmlNode* node) {
    while (node) {
        if (node->type != XML_TEXT_NODE) {
            return true;
        }
        node = node->next;
    }
    return false;
}

void XmlOperation::RecursiveMerge(xmlNode *game_node, xmlNode *patching_node)
{
    if (!patching_node) {
        return;
    }
    const auto find_node_with_name = [](auto game_node, auto name) -> xmlNode * {
        for (auto cur_node = game_node; cur_node; cur_node = cur_node->next) {
            if (xmlStrcmp(cur_node->name, name) == 0) {
                return cur_node;
            }
        }
        return nullptr;
    };

    if (HasNonTextNode(patching_node)) {
        while (patching_node && patching_node->type == XML_TEXT_NODE) {
            patching_node = patching_node->next;
        }
    }

    if (HasNonTextNode(game_node)) {
        while (game_node && game_node->type == XML_TEXT_NODE) {
            game_node = game_node->next;
        }
    }

    xmlNode *prev_game_node = nullptr;
    for (auto cur_node = patching_node; cur_node; cur_node = cur_node->next) {
        if (game_node && game_node->type != XML_TEXT_NODE) {
            prev_game_node = game_node;
        }
        game_node = find_node_with_name(game_node, cur_node->name);
        MergeProperties(game_node, cur_node);
        if (game_node) {
            if (game_node->type == XML_TEXT_NODE) {
                xmlNodeSetContent(game_node, cur_node->content);
            } else {
                RecursiveMerge(game_node->children, cur_node->children);
            }
        } else {
            if (cur_node && prev_game_node) {
                while (prev_game_node) {
                    RecursiveMerge(prev_game_node->children, cur_node);
                    prev_game_node = prev_game_node->next;
                }
            }
        }

    }
}
