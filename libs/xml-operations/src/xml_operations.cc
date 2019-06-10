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
        path_ = "/AssetList/Groups/Group/Assets/Asset[Values/Standard/GUID='" + guid + "']";
        speculative_path_ =
            "/AssetList/Groups/Group[1]/Assets/Asset[Values/Standard/GUID='" + guid + "']";
    }
    auto prop_path = GetXmlPropString(node, "Path");
    if (prop_path.find("/") != 0) {
        path_ += "/";
        speculative_path_ += "/";
    }
    path_ += GetXmlPropString(node, "Path");
    speculative_path_ += GetXmlPropString(node, "Path");
    if (path_ == "/") {
        path_ = "/*";
    }
    if (speculative_path_ == "/") {
        speculative_path_ = "/*";
    }
    if (path_.length() > 0) {
        if (path_[path_.length() - 1] == '/') {
            path_ = path_.substr(0, path_.length() - 1);
        }
    }
    if (speculative_path_.length() > 0) {
        if (speculative_path_[speculative_path_.length() - 1] == '/') {
            speculative_path_ = speculative_path_.substr(0, speculative_path_.length() - 1);
        }
    }

    if (path_.find("//Assets[") == 0) {
        auto npath        = path_.substr(strlen("//Assets["));
        path_             = "/AssetList/Groups/Group/Assets[" + npath;
        speculative_path_ = "/AssetList/Groups/Group[1]/Assets[" + npath;
    } else if (path_.find("//Asset") == 0) {
        auto npath        = path_.substr(strlen("//Asset"));
        path_             = "/AssetList/Groups/Group/Assets/Asset" + npath;
        speculative_path_ = "/AssetList/Groups/Group[1]/Assets/Asset" + npath;
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

void XmlOperation::Apply(std::shared_ptr<pugi::xml_document> doc, fs::path mod_path)
{
    try {
        pugi::xpath_node_set results;
        if (!speculative_path_.empty()) {
            results = doc->select_nodes(speculative_path_.c_str());
        }
        if (results.empty()) {
            results = doc->select_nodes(GetPath().c_str());
        }
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
                RecursiveMerge(game_node, game_node, patching_node);
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
    } catch (const pugi::xpath_exception &e) {
        spdlog::error("Failed to parse path {} in {}: ", GetPath(), mod_path.string(), e.what());
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

using offset_data_t = std::vector<ptrdiff_t>;
static bool build_offset_data(offset_data_t &result, const char *file)
{
    FILE *f = fopen(file, "rb");
    if (!f)
        return false;

    ptrdiff_t offset = 0;

    char   buffer[1024];
    size_t size;

    while ((size = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        for (size_t i = 0; i < size; ++i)
            if (buffer[i] == '\n')
                result.push_back(offset + i);

        offset += size;
    }

    fclose(f);

    return true;
}

static std::pair<size_t, size_t> get_location(const offset_data_t &data, ptrdiff_t offset)
{
    offset_data_t::const_iterator it    = std::lower_bound(data.begin(), data.end(), offset);
    size_t                        index = it - data.begin();

    return std::make_pair(1 + index, index == 0 ? offset + 1 : offset - data[index - 1]);
}

std::vector<XmlOperation> XmlOperation::GetXmlOperationsFromFile(fs::path path)
{
    std::shared_ptr<pugi::xml_document> doc          = std::make_shared<pugi::xml_document>();
    auto                                parse_result = doc->load_file(path.string().c_str());
    if (!parse_result) {
        offset_data_t offset_data;
        build_offset_data(offset_data, path.string().c_str());
        auto location = get_location(offset_data, parse_result.offset);
        spdlog::error("Failed to parse {}({}, {}): {}", path.string(), location.first,
                      location.second, parse_result.description());
    }
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

void XmlOperation::RecursiveMerge(pugi::xml_node root_game_node, pugi::xml_node game_node,
                                  pugi::xml_node patching_node)
{
    if (!patching_node) {
        return;
    }

    const auto find_node_with_name = [](pugi::xml_node game_node, auto name) -> pugi::xml_node {
        if (game_node.name() == std::string(name)) {
            return game_node;
        }
        auto children = game_node.children();
        for (pugi::xml_node cur_node : children) {
            if (cur_node.name() == std::string(name)) {
                return cur_node;
            }
        }
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
                RecursiveMerge(root_game_node, game_node.first_child(), cur_node.first_child());
            }
        } else {
            if (cur_node && prev_game_node) {
                while (prev_game_node) {
                    RecursiveMerge(root_game_node, prev_game_node.first_child(), cur_node);
                    if (prev_game_node == game_node) {
                        break;
                    }
                    prev_game_node = prev_game_node.next_sibling();
                }
            }
        }
    }
}
