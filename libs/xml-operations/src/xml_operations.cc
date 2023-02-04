#include "xml_operations.h"

#include "absl/strings/str_split.h"
#include "spdlog/spdlog.h"

#include <cstdio>
#include <cstring>
#include <regex>

using offset_data_t = std::vector<ptrdiff_t>;

namespace
{
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
} // namespace

XmlOperation::XmlOperation(std::shared_ptr<pugi::xml_document> doc, pugi::xml_node node,
                           std::string guid, std::string temp, std::string mod_name,
                           fs::path game_path, fs::path mod_path, fs::path mod_base_path)
{
    guid_     = guid;
    template_ = temp;
    node_     = node;
    doc_      = doc;

    mod_name_      = mod_name;
    game_path_     = game_path;
    mod_path_      = mod_path;
    mod_base_path_ = mod_base_path;

    ReadPath(node, guid, temp);
    ReadType(node, mod_name, game_path, mod_path, mod_base_path);
    if (type_ != Type::Remove) {
        nodes_ = node.children();
    }

    skip_ = node.attribute("Skip");
    condition_ = node.attribute("Condition").as_string();
    allow_no_match_ = node.attribute("AllowNoMatch");
}

void XmlOperation::ReadPath(pugi::xml_node node, std::string guid, std::string temp)
{
    auto prop_path = GetXmlPropString(node, "Path");
    if (prop_path.empty()) {
        prop_path = "/";
    }

    if (prop_path.find('&') != -1)
    {
        prop_path = std::regex_replace(prop_path, std::regex{"&gt;"}, ">");
        prop_path = std::regex_replace(prop_path, std::regex{"&lt;"}, "<");
    }

    if (guid.empty()) {
        // Rewrite path to use faster GUID lookup
        int g;
        char s[256];
        // Matches stuff like this and extracts GUID //Assets[Asset/Values/Standard/GUID='102119']
        if (sscanf(prop_path.c_str(), "//Assets[Asset/Values/Standard/GUID='%d']", &g) > 0) {
            if (std::string("//Assets[Asset/Values/Standard/GUID='") + std::to_string(g) + "']"
                == prop_path) {
                guid                   = std::to_string(g);
                guid_                  = std::to_string(g);
                speculative_path_type_ = SpeculativePathType::ASSET_CONTAINER;
            }
        }
        //else if (sscanf(prop_path.c_str(), "//Asset[Values/Standard/GUID='%d']%s", &g, s) > 0) {
        //    if (std::string("//Asset[Values/Standard/GUID='") + std::to_string(g) + "']" + s
        //        == prop_path) {
        //        guid = std::to_string(g);
        //        guid_ = std::to_string(g);
        //        speculative_path_type_ = SpeculativePathType::SINGLE_ASSET;
        //        prop_path = s;
        //        path_ = "//Asset[Values/Standard/GUID='" + guid + "']";
        //    }
        //    else {
        //        spdlog::warn("Failed to construct speculative path lookup");
        //    }
        //}
    } else {
        speculative_path_type_ = SpeculativePathType::SINGLE_ASSET;
        path_                  = "//Asset[Values/Standard/GUID='" + guid + "']";
    }

    if (temp.empty()) {
        char g[256];
        if (sscanf(prop_path.c_str(), "//Template[Name='%s']", g) > 0) {
            if (std::string("//Template[Name='" + std::string(g) + "']") == prop_path) {
                temp                   = g;
                template_              = g;
                speculative_path_type_ = SpeculativePathType::TEMPLATE_CONTAINER;
            }
        }
    } else {
        speculative_path_type_ = SpeculativePathType::SINGLE_TEMPLATE;
        path_                  = "//Template[Name='" + temp + "']";
    }

    if (prop_path.find("/") != 0) {
        path_ += "/";
    }
    path_ += prop_path;
    if (path_ == "/") {
        path_ = "/*";
    }
    if (path_.length() > 0) {
        if (path_[path_.length() - 1] == '/') {
            path_ = path_.substr(0, path_.length() - 1);
        }
    }

    if (!guid.empty() || !temp.empty()) {
        if (speculative_path_type_ == SpeculativePathType::ASSET_CONTAINER
            || speculative_path_type_ == SpeculativePathType::TEMPLATE_CONTAINER) {
            speculative_path_ = "/";
        } else {
            speculative_path_ += prop_path;
        }

        if (speculative_path_ == "/") {
            speculative_path_ = "self::node()";
        }

        if (speculative_path_.length() > 0) {
            if (speculative_path_[speculative_path_.length() - 1] == '/') {
                speculative_path_ = speculative_path_.substr(0, speculative_path_.length() - 1);
            }
        }

        if (speculative_path_.find("/") == 0) {
            speculative_path_ = speculative_path_.substr(1);
        }
    }
}

void XmlOperation::ReadType(pugi::xml_node node, std::string mod_name, fs::path game_path,
                            fs::path mod_path, fs::path mod_base_path)
{
#ifndef _WIN32
    auto stricmp = [](auto a, auto b) { return strcasecmp(a, b); };
#endif
    auto type = GetXmlPropString(node, "Type");
    if (stricmp(type.c_str(), "add") == 0) {
        type_ = Type::Add;
    } else if (stricmp(type.c_str(), "addNextSibling") == 0) {
        type_ = Type::AddNextSibling;
    } else if (stricmp(type.c_str(), "addPrevSibling") == 0) {
        type_ = Type::AddPrevSibling;
    } else if (stricmp(type.c_str(), "add") == 0) {
        type_ = Type::Add;
    } else if (stricmp(type.c_str(), "remove") == 0) {
        type_ = Type::Remove;
    } else if (stricmp(type.c_str(), "replace") == 0) {
        type_ = Type::Replace;
    } else if (stricmp(type.c_str(), "merge") == 0) {
        type_ = Type::Merge;
    } else {
        type_ = Type::None;
        offset_data_t offset_data;
        build_offset_data(offset_data, (mod_base_path / mod_path).string().c_str());
        auto [line, column] = get_location(offset_data, node_.offset_debug());
        spdlog::warn("No matching node for Path {} in {} ({}:{})", GetPath(), mod_name,
                     mod_path_.string(), line);
        spdlog::error("Unknown ModOp({}), ignoring {}", type, GetPath());
    }
}

std::optional<pugi::xml_node> XmlOperation::FindAsset(std::string guid, pugi::xml_node node)
{
#ifndef _WIN32
    auto stricmp = [](auto a, auto b) { return strcasecmp(a, b); };
#endif
    //
    if (stricmp(node.name(), "Asset") == 0) {
        auto values = node.child("Values");
        if (!values) {
            return {};
        }

        auto standard = values.child("Standard");
        if (!standard) {
            return {};
        }

        auto GUID = standard.child("GUID");
        if (!GUID) {
            return {};
        }

        if (GUID.text().get() != guid) {
            return {};
        }
        if (speculative_path_type_ == SpeculativePathType::ASSET_CONTAINER) {
            auto parent = node.parent();
            while (parent && stricmp(parent.name(), "Assets") != 0) {
                parent = parent.parent();
            }
            if (stricmp(parent.name(), "Assets") == 0) {
                return parent;
            }
            return {};
        } else {
            return node;
        }
    }

    for (pugi::xml_node n : node.children()) {
        if (auto found = FindAsset(guid, n); found) {
            return found;
        }
    }

    return {};
}

std::optional<pugi::xml_node> XmlOperation::FindAsset(std::shared_ptr<pugi::xml_document> doc,
                                                      std::string                         guid)
{
    return FindAsset(guid, doc->root());
}

std::optional<pugi::xml_node> XmlOperation::FindTemplate(std::string temp, pugi::xml_node node)
{
#ifndef _WIN32
    auto stricmp = [](auto a, auto b) { return strcasecmp(a, b); };
#endif
    //
    if (stricmp(node.name(), "Template") == 0) {
        auto template_name = node.child("Name");
        if (!template_name) {
            return {};
        }

        if (template_name.text().get() != temp) {
            return {};
        }

        if (speculative_path_type_ == SpeculativePathType::TEMPLATE_CONTAINER) {
            auto parent = node.parent();
            while (parent && stricmp(parent.name(), "Templates") != 0) {
                parent = parent.parent();
            }
            if (stricmp(parent.name(), "Templates") == 0) {
                return parent;
            }
            return {};
        } else {
            return node;
        }
    }

    for (pugi::xml_node n : node.children()) {
        if (auto found = FindTemplate(temp, n); found) {
            return found;
        }
    }

    return {};
}

std::optional<pugi::xml_node> XmlOperation::FindTemplate(std::shared_ptr<pugi::xml_document> doc,
                                                         std::string                         temp)
{
    return FindTemplate(temp, doc->root());
}

pugi::xpath_node_set XmlOperation::ReadGuidNodes(std::shared_ptr<pugi::xml_document> doc)
{
    pugi::xpath_node_set results;
    if (!guid_.empty()) {
        try {
            auto node = FindAsset(doc, guid_);
            if (node) {
                if (speculative_path_ != "*") {
                    results = node->select_nodes(speculative_path_.c_str());
                }
            } else {
                spdlog::debug("Speculative path failed to find node {}", GetPath());
            }
            if (results.empty()) {
                spdlog::debug("Speculative path failed to find node with path {} {}", GetPath(),
                              speculative_path_);
            }
        } catch (const pugi::xpath_exception &e) {
            spdlog::warn("Speculative path lookup failed {} (GUID={}) in {}: {}. Please create "
                         "an issue with the mod op that caused this! Falling back to regular "
                         "'slow' lookup.",
                         speculative_path_, guid_, mod_path_.string(), e.what());
        }
    }
    return results;
}

pugi::xpath_node_set XmlOperation::ReadTemplateNodes(std::shared_ptr<pugi::xml_document> doc)
{
    pugi::xpath_node_set results;
    if (!template_.empty()) {
        try {
            auto node = FindTemplate(doc, template_);
            if (node) {
                if (speculative_path_ != "*") {
                    results = node->select_nodes(speculative_path_.c_str());
                }
            } else {
                spdlog::debug("Speculative path failed to find node {}", GetPath());
            }
            if (results.empty()) {
                spdlog::debug("Speculative path failed to find node with path {} {}", GetPath(),
                              speculative_path_);
            }
        } catch (const pugi::xpath_exception &e) {
            spdlog::warn("Speculative path lookup failed {} (Template={}) in {}: {}. Please create "
                         "an issue with the mod op that caused this! Falling back to regular "
                         "'slow' lookup.",
                         speculative_path_, template_, mod_path_.string(), e.what());
        }
    }
    return results;
}

void XmlOperation::Apply(std::shared_ptr<pugi::xml_document> doc)
{
    if (skip_ || GetType() == XmlOperation::Type::None) {
        return;
    }

    if (!CheckCondition(doc)) {
        return;
    }

    try {
        spdlog::debug("Looking up {}", path_);
        pugi::xpath_node_set results = ReadGuidNodes(doc);

        if (results.empty()) {
            results = ReadTemplateNodes(doc);
        }

        if (results.empty()) {
            results = doc->select_nodes(GetPath().c_str());
        }
        if (results.empty()) {
            offset_data_t offset_data;
            build_offset_data(offset_data, (mod_base_path_ / mod_path_).string().c_str());
            auto [line, column] = get_location(offset_data, node_.offset_debug());

            const char* msg = "No matching node for Path {} in {} ({}:{})";
            if (allow_no_match_) {
                spdlog::debug(msg, GetPath(), mod_name_, mod_path_.string(), line);
            }
            else {
                spdlog::warn(msg, GetPath(), mod_name_, mod_path_.string(), line);
            }

            return;
        }

        spdlog::debug("Lookup finished {}", path_);
        for (pugi::xpath_node xnode : results) {
            pugi::xml_node game_node = xnode.node();
            if (GetType() == XmlOperation::Type::Merge) {
                for (auto& node : GetContentNode()) {
                    if (strcmp(node.name(), game_node.name()) == 0) {
                        // legacy support
                        offset_data_t offset_data;
                        build_offset_data(offset_data, mod_path_.string().c_str());
                        auto [line, column] = get_location(offset_data, node_.offset_debug());
                        spdlog::warn("Deprecated: remove <{}> parent from merge in {} ({}:{})", node.name(), mod_name_, mod_path_.string(), line);
                        RecursiveMerge(game_node, game_node.parent(), node);
                        continue;
                    }

                    RecursiveMerge(game_node, game_node, node);
                }
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
        }
    } catch (const pugi::xpath_exception &e) {
        spdlog::error("Failed to parse path {} in {}: {}", GetPath(), mod_path_.string(), e.what());
    }
}

std::vector<XmlOperation> XmlOperation::GetXmlOperations(std::shared_ptr<pugi::xml_document> doc,
                                                         std::function<std::shared_ptr<pugi::xml_document>(fs::path)> include_loader,
                                                         fs::path mod_relative_path,
                                                         std::string mod_name, 
                                                         fs::path game_path,
                                                         fs::path mod_path)
{
#ifndef _WIN32
    auto stricmp = [](auto a, auto b) { return strcasecmp(a, b); };
#endif
    pugi::xml_node root = doc->root();
    if (!root) {
        spdlog::error("Failed to get root element");
        return {};
    }
    std::vector<XmlOperation> mod_operations;
    if (stricmp(root.first_child().name(), "ModOps") == 0) {
        for (pugi::xml_node node : root.first_child().children()) {
            if (node.type() == pugi::xml_node_type::node_element) {
                if (stricmp(node.name(), "ModOp") == 0) {
                    const auto guid = GetXmlPropString(node, "GUID");
                    const auto temp = GetXmlPropString(node, "Template");
                    std::vector<std::string> guids;
                    if (!temp.empty() && !guid.empty()) {
                        spdlog::error("Cannot supply both `Template` and `GUID`");
                    }
                    if (!guid.empty()) {
                        std::vector<std::string> guids = absl::StrSplit(guid, ',');
                        for (auto g : guids) {
                            mod_operations.emplace_back(doc, node, g.data(), "", mod_name, game_path, 
                                                        mod_relative_path, mod_path);
                        }   
                    } else if (!temp.empty()) {
                        mod_operations.emplace_back(doc, node, "", temp, mod_name, game_path, mod_relative_path, 
                                                    mod_path);
                    } else {
                        mod_operations.emplace_back(doc, node, "", "", mod_name, game_path, mod_relative_path, 
                                                    mod_path);
                    }
                } else if (stricmp(node.name(), "Include") == 0) {
                    const auto file = GetXmlPropString(node, "File");
                    fs::path relative_include_path;
                    if (file.rfind("/", 0) == 0) {
                        relative_include_path = file.substr(1);
                    } else {
                        relative_include_path = (mod_relative_path.parent_path() / file).lexically_normal();
                    }

                    const bool skip = node.attribute("Skip");
                    if (!skip) {
                        const auto include_doc = include_loader(relative_include_path);
                        auto include_ops = GetXmlOperations(include_doc, 
                                                            include_loader,
                                                            relative_include_path,
                                                            mod_name, 
                                                            game_path, 
                                                            mod_path);
                        mod_operations.insert(std::end(mod_operations), std::begin(include_ops),
                                            std::end(include_ops));
                    }
                }
            }
        }
    } else {
        spdlog::warn("[{}] Mod doesn't contain ModOps root node", mod_name);
    }
    return mod_operations;
}

std::vector<XmlOperation> XmlOperation::GetXmlOperationsFromFile(fs::path    file_path,
                                                                 std::string mod_name,
                                                                 fs::path    game_path,
                                                                 fs::path    mod_path)
{
    auto doc          = std::make_shared<pugi::xml_document>();
    auto parse_result = doc->load_file(file_path.string().c_str());
    if (!parse_result) {
        offset_data_t offset_data;
        build_offset_data(offset_data, file_path.string().c_str());
        auto location = get_location(offset_data, parse_result.offset);
        spdlog::error("[{}] Failed to parse {}({}, {}): {}", mod_name, file_path.string(),
                      location.first, location.second, parse_result.description());
        return {};
    }

    const auto include_loader = [&mod_path](fs::path filePath) {
        std::shared_ptr<pugi::xml_document> doc = std::make_shared<pugi::xml_document>();
        auto parse_result = doc->load_file((mod_path / filePath).string().c_str());
        return doc;
    };
    const auto mod_relative_path = file_path.lexically_relative(mod_path);
    return GetXmlOperations(doc, include_loader, mod_relative_path, mod_name, game_path, mod_path);
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

    const auto find_node_with_name = [&root_game_node](pugi::xml_node game_node, auto name) -> pugi::xml_node {
        auto children = game_node.children();
        for (pugi::xml_node cur_node : children) {
            if (strcmp(cur_node.name(), name) == 0) {
                return cur_node;
            }
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

    auto root_node = game_node;

    pugi::xml_node prev_game_node;
    for (auto cur_node = patching_node; cur_node; cur_node = cur_node.next_sibling()) {
        game_node = find_node_with_name(root_node, cur_node.name());
        if (game_node) {
            if (cur_node.type() == pugi::xml_node_type::node_pcdata) {
                game_node.set_value(cur_node.value());
            } else {
                MergeProperties(game_node, cur_node);
                for (auto& child : cur_node.children()) {
                    RecursiveMerge(root_game_node, game_node, child);
                }
            }
        }
        else {
            root_node.append_copy(cur_node);
        }
    }
}

bool XmlOperation::CheckCondition(std::shared_ptr<pugi::xml_document> doc)
{
    if (condition_.empty()) {
        return true;
    }

    try {
        bool negative_match = condition_[0] == '!';
        auto condition_path = condition_.c_str() + (negative_match ? 1 : 0);

        const auto match_nodes = doc->select_nodes(condition_path);
        if (negative_match != match_nodes.empty()) {
            offset_data_t offset_data;
            build_offset_data(offset_data, (mod_base_path_ / mod_path_).string().c_str());
            auto [line, column] = get_location(offset_data, node_.offset_debug());
            spdlog::debug("Condition not matching {} in {} ({}:{})", condition_, mod_name_,
                        mod_path_.string(), line);
            return false;
        }
    } catch (const pugi::xpath_exception &e) {
        spdlog::error("Failed to parse path {} in {}: {}", condition_, mod_path_.string(), e.what());
    }

    return true;
}

std::string XmlOperation::GetPath()
{
    return path_;
}

pugi::xml_object_range<pugi::xml_node_iterator> XmlOperation::GetContentNode()
{
    return *nodes_;
}

XmlOperation::Type XmlOperation::GetType() const
{
    return type_;
}
