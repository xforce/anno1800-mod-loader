#include "xml_operations.h"

#include "absl/strings/str_split.h"
#include "spdlog/spdlog.h"

#include <cstdio>

XmlOperation::XmlOperation(std::shared_ptr<pugi::xml_document> doc, pugi::xml_node node)
{
    doc_ = doc;
    ReadType(node);
    ReadPath(node);
    if (type_ != Type::Remove) {
        nodes_ = node.children();
    }
}

XmlOperation::XmlOperation(std::shared_ptr<pugi::xml_document> doc, pugi::xml_node node,
                           std::string guid)
{
    doc_ = doc;
    ReadPath(node, guid);
    ReadType(node);
    if (type_ != Type::Remove) {
        nodes_ = node.children();
    }
}

void XmlOperation::ReadPath(pugi::xml_node node, std::string guid)
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
}

void XmlOperation::ReadType(pugi::xml_node node)
{
    auto type = GetXmlPropString(node, "Type");
    if (type == "add") {
        type_ = Type::Add;
    } else if (type == "addNextSibling") {
        type_ = Type::AddNextSibling;
    } else if (type == "addPrevSibling") {
        type_ = Type::AddPrevSibling;
    } else if (type == "add") {
        type_ = Type::Add;
    } else if (type == "remove") {
        type_ = Type::Remove;
    } else if (type == "replace") {
        type_ = Type::Replace;
    } else if (type == "merge") {
        type_ = Type::Merge;
    }
}

void XmlOperation::Apply(std::shared_ptr<pugi::xml_document> doc)
{
    pugi::xpath_node_set results =
        doc->select_nodes((std::string("/MEOW_XML_SUCKS") + GetPath()).c_str());
    if (results.empty()) {
        spdlog::warn("No matching node for Path {}", GetPath());
        return;
    }
    for (pugi::xpath_node xnode : results) {
        pugi::xml_node game_node = xnode.node();
        if (GetType() == XmlOperation::Type::Merge) {
            auto content_node = GetContentNode();
            if (content_node.begin() == content_node.end()) {
                //
                continue;
            }
            pugi::xml_node patching_node = *content_node.begin();
            RecursiveMerge(game_node, patching_node);
        } else if (GetType() == XmlOperation::Type::AddNextSibling) {
            for (auto &&node : GetContentNode()) {
                game_node = game_node.parent().insert_copy_after(node, game_node);
            }
        } else if (GetType() == XmlOperation::Type::AddPrevSibling) {
            for (auto &&node : GetContentNode()) {
                game_node.parent().insert_copy_before(node, game_node);
            }
        } else if (GetType() == XmlOperation::Type::Add) {
            for (auto &node : GetContentNode()) {
                game_node.append_copy(node);
            }
        } else if (GetType() == XmlOperation::Type::Remove) {
            game_node.parent().remove_child(game_node);
        } else if (GetType() == XmlOperation::Type::Replace) {
            for (auto &node : GetContentNode()) {
                game_node.parent().insert_copy_after(node, game_node);
            }
            game_node.parent().remove_child(game_node);
        }
        //
    }
}

std::vector<XmlOperation> XmlOperation::GetXmlOperations(std::shared_ptr<pugi::xml_document> doc)
{
    pugi::xml_node root = doc->root();
    if (!root) {
        spdlog::error("Failed to get root element");
        return {};
    }
    std::vector<XmlOperation> mod_operations;
    if (root.first_child().name() == std::string("ModOps")) {
        for (pugi::xml_node node : root.first_child().children()) {
            if (node.type() == pugi::xml_node_type::node_element) {
                if (node.name() == std::string("ModOp")) {
                    const auto guid = GetXmlPropString(node, "GUID");
                    if (!guid.empty()) {
                        std::vector<std::string> guids = absl::StrSplit(guid, ',');
                        for (auto g : guids) {
                            mod_operations.emplace_back(doc, node, g.data());
                        }
                    } else {
                        mod_operations.emplace_back(doc, node);
                    }
                }
            }
        }
    }
    return mod_operations;
}

std::vector<XmlOperation> XmlOperation::GetXmlOperationsFromFile(fs::path path)
{
    std::shared_ptr<pugi::xml_document> doc = std::make_shared<pugi::xml_document>();
    doc->load_file(path.string().c_str());
    return GetXmlOperations(doc);
}

void MergeProperties(pugi::xml_node game_node, pugi::xml_node patching_node)
{
    for (pugi::xml_attribute &attr : patching_node.attributes()) {
        if (auto at = game_node.find_attribute(
                [attr](auto x) { return std::string(x.name()) == attr.name(); });
            at) {
            game_node.remove_attribute(at);
        }
        game_node.append_attribute(attr.name()).set_value(attr.value());
    }
}

static bool HasNonTextNode(pugi::xml_node node)
{
    while (node) {
        if (node.type() != pugi::xml_node_type::node_pcdata) {
            return true;
        }
        node = node.next_sibling();
    }
    return false;
}

void XmlOperation::RecursiveMerge(pugi::xml_node game_node, pugi::xml_node patching_node)
{
    if (!patching_node) {
        return;
    }

    const auto find_node_with_name = [](pugi::xml_node game_node, auto name) -> pugi::xml_node {
        auto cur_node = game_node;
        while (cur_node) {
            if (cur_node.name() == std::string(name)) {
                return cur_node;
            }
            cur_node = cur_node.next_sibling();
        }
        return {};
    };

    if (HasNonTextNode(patching_node)) {
        while (patching_node && patching_node.type() == pugi::xml_node_type::node_pcdata) {
            patching_node = patching_node.next_sibling();
        }
    }

    if (HasNonTextNode(game_node)) {
        while (game_node && game_node.type() == pugi::xml_node_type::node_pcdata) {
            game_node = game_node.next_sibling();
        }
    }

    pugi::xml_node prev_game_node;
    for (auto cur_node = patching_node; cur_node; cur_node = cur_node.next_sibling()) {
        if (game_node && game_node.type() != pugi::xml_node_type::node_pcdata) {
            prev_game_node = game_node;
        }
        game_node = find_node_with_name(game_node, cur_node.name());
        MergeProperties(game_node, cur_node);
        if (game_node) {
            if (game_node.type() == pugi::xml_node_type::node_pcdata) {
                game_node.set_value(cur_node.value());
            } else {
                RecursiveMerge(game_node.first_child(), cur_node.first_child());
            }
        } else {
            if (cur_node && prev_game_node) {
                while (prev_game_node) {
                    RecursiveMerge(prev_game_node.first_child(), cur_node);
                    prev_game_node = prev_game_node.next_sibling();
                }
            }
        }
    }
}
