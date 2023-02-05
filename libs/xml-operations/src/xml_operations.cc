#include "xml_operations.h"

#include "absl/strings/str_split.h"
#include "spdlog/spdlog.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>

XmlOperationContext::XmlOperationContext() { }

XmlOperationContext::XmlOperationContext(const fs::path& mod_relative_path,
                                         const fs::path& mod_base_path,
                                         std::string_view mod_name)
{
    include_loader_ = [this, &mod_base_path, &mod_name](fs::path file_path) {
        std::vector<char> buffer;
        size_t size;
        if (!ReadFile(mod_base_path / file_path, buffer, size)) {
            spdlog::error("{}: Failed to open {}",
                          mod_name.empty() ? mod_base_path.string() : mod_name, 
                          file_path.string());
            return XmlOperationContext{};
        }
        return XmlOperationContext{buffer.data(), size, this->include_loader_, file_path};
    };

    *this = include_loader_(mod_relative_path);
    mod_name_ = mod_name;
}

XmlOperationContext::XmlOperationContext(const char* buffer, size_t size,
                                         include_loader_t include_loader,
                                         const fs::path& doc_path,
                                         std::string_view mod_name)
{
    mod_name_ = mod_name;
    include_loader_ = include_loader;
    doc_path_ = doc_path;

    offset_data_ = BuildOffsetData(buffer, size);
    doc_ = std::make_shared<pugi::xml_document>();
    auto parse_result = doc_->load_buffer(buffer, size);
    if (!parse_result) {
        spdlog::error("{}: Failed to parse {} (line {}): {}",
                      mod_name, doc_path.string(),
                      this->GetLine(parse_result.offset), parse_result.description());
    }
}

inline XmlOperationContext XmlOperationContext::OpenInclude(fs::path file_path) const
{ 
    auto include = include_loader_(file_path);
    include.mod_name_ = mod_name_;
    return include; 
}

size_t XmlOperationContext::GetLine(ptrdiff_t offset) const
{
    auto it = std::lower_bound(offset_data_.begin(), offset_data_.end(), offset);
    return 1 + it - offset_data_.begin();
}

pugi::xml_node XmlOperationContext::GetRoot() const
{
#ifndef _WIN32
    auto stricmp = [](auto a, auto b) { return strcasecmp(a, b); };
#endif

    auto root = doc_ ? doc_->root() : pugi::xml_node{};
    if (!root) {
        spdlog::error("Failed to get root element");
        return {};
    }
    
    if (!root.first_child() || stricmp(root.first_child().name(), "ModOps") != 0) {
        Error("Doesn't contain ModOps root node");
        return {};
    }

    return root.first_child();
}

template<typename... Args>
void XmlOperationContext::Debug(std::string_view msg, const Args &... args) const
{
    spdlog::debug(msg, args...);
}

void XmlOperationContext::Warn(std::string_view msg, pugi::xml_node node) const
{
    spdlog::warn("{}: {} ({}:{})", mod_name_, msg, doc_path_.string(), node ? GetLine(node) : 0);
}

void XmlOperationContext::Error(std::string_view msg, pugi::xml_node node) const
{
    spdlog::error("{}: {} ({}:{})", mod_name_, msg, doc_path_.string(), node ? GetLine(node) : 0);
}

bool XmlOperationContext::ReadFile(const fs::path& file_path, std::vector<char>& buffer, size_t& size)
{
    std::ifstream ifs(file_path, std::ios::in | std::ios::ate);
    if (!ifs) {
        return false;
    }

    size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    buffer.resize(size);
    ifs.read(buffer.data(), size);
    return true;
}

XmlOperationContext::offset_data_t XmlOperationContext::BuildOffsetData(const char* buffer, size_t size)
{
    offset_data_t result;
    for (size_t i = 0; i < size; ++i) {
        if (buffer[i] == '\n') {
            result.push_back(i);
        }
    }
    return result;
}

XmlOperation::XmlOperation(XmlOperationContext doc, pugi::xml_node node,
                           std::string guid, std::string temp, std::string mod_name,
                           fs::path game_path) : doc_(doc)
{
    guid_     = guid;
    template_ = temp;
    node_     = node;

    mod_name_      = mod_name;
    game_path_     = game_path;
    mod_path_      = doc.GetPath();

    ReadPath(node, guid, temp);
    ReadType(node, mod_name, game_path);
    if (type_ != Type::Remove) {
        nodes_ = node.children();
    }

    condition_ = node.attribute("Condition").as_string();
    allow_no_match_ = node.attribute("AllowNoMatch");

    if (type_ != Type::Remove) {
        content_ = node.attribute("Content").as_string();
        if (!content_.empty() && nodes_->begin() != nodes_->end()) {
            doc_.Error("ModOp must be empty when Content is used", node_);
            nodes_ = {};
        }
    }
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

void XmlOperation::ReadType(pugi::xml_node node, std::string mod_name, fs::path game_path)
{
#ifndef _WIN32
    auto stricmp = [](auto a, auto b) { return strcasecmp(a, b); };
#endif
    auto type = GetXmlPropString(node, "Type");

    if (stricmp(node.name(), "Include") == 0 ||
        stricmp(node.name(), "Group") == 0) {
        type_ = Type::Group;
    }
    else if (stricmp(type.c_str(), "add") == 0) {
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
        doc_.Warn("No matching node for Path " + GetPath(), node_);
        doc_.Error("Unknown ModOp " + type, node_);
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
            }
            if (results.empty()) {
                doc_.Debug("Speculative path failed to find node with path {} {}", GetPath(),
                           speculative_path_);
            }
        } catch (const pugi::xpath_exception& e) {
            doc_.Error("Speculative path failed to find node with path '" + GetPath() +
                       "' " + speculative_path_);
            doc_.Error(e.what());
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
            if (node && speculative_path_ != "*") {
                results = node->select_nodes(speculative_path_.c_str());
            }
            if (results.empty()) {
                doc_.Debug("Speculative path failed to find node with path {} {}", GetPath(),
                           speculative_path_);
            }
        } catch (const pugi::xpath_exception& e) {
            doc_.Error("Speculative path failed to find node with path '" + GetPath() +
                       "' " + speculative_path_);
            doc_.Error(e.what());
        }
    }
    return results;
}

void XmlOperation::Apply(std::shared_ptr<pugi::xml_document> doc)
{
    if (GetType() == XmlOperation::Type::None) {
        return;
    }

    if (!CheckCondition(doc)) {
        return;
    }

    if (type_ == Type::Group) {
        for (auto& modop : group_) {
            modop.Apply(doc);
        }
        return;
    }

    std::vector<pugi::xml_node> content_nodes;
    if (type_ != Type::Remove && !content_.empty()) {
        pugi::xpath_node_set result;
        if (result.empty()) {
            result = doc->select_nodes(content_.c_str());
        }
        if (result.empty()) {
            doc_.Warn("No matching node for Path" + GetPath(), node_);
            return;
        }
        for (auto& node : result)
            content_nodes.push_back(node.node());
    }
    if (content_.empty() && nodes_) {
        content_nodes.insert(content_nodes.end(), nodes_->begin(), nodes_->end());
    }

    try {
        doc_.Debug("Looking up {}", path_);
        pugi::xpath_node_set results = ReadGuidNodes(doc);

        if (results.empty()) {
            results = ReadTemplateNodes(doc);
        }

        if (results.empty()) {
            results = doc->select_nodes(GetPath().c_str());
        }
        if (results.empty()) {
            if (allow_no_match_) {
                doc_.Debug("No matching node for Path {}", GetPath());
            }
            else {
                doc_.Warn("No matching node for Path " + GetPath(), node_);
            }

            return;
        }

        doc_.Debug("Lookup finished {}", path_);
        for (pugi::xpath_node xnode : results) {
            pugi::xml_node game_node = xnode.node();
            if (GetType() == XmlOperation::Type::Merge) {
                if (content_nodes.size() == 1 &&
                    strcmp(content_nodes.begin()->name(), game_node.name()) == 0) {
                    // legacy merge
                    // skip single container if it's named same as the target node
                    RecursiveMerge(game_node, game_node.parent(), *content_nodes.begin());
                }
                else {
                    for (auto& node : content_nodes) {
                        RecursiveMerge(game_node, game_node, node);
                    }
                }
            } else if (GetType() == XmlOperation::Type::AddNextSibling) {
                for (auto &&node : content_nodes) {
                    game_node = game_node.parent().insert_copy_after(node, game_node);
                }
            } else if (GetType() == XmlOperation::Type::AddPrevSibling) {
                for (auto &&node : content_nodes) {
                    game_node.parent().insert_copy_before(node, game_node);
                }
            } else if (GetType() == XmlOperation::Type::Add) {
                for (auto &node : content_nodes) {
                    game_node.append_copy(node);
                }
            } else if (GetType() == XmlOperation::Type::Remove) {
                game_node.parent().remove_child(game_node);
            } else if (GetType() == XmlOperation::Type::Replace) {
                for (auto &node : content_nodes) {
                    game_node.parent().insert_copy_after(node, game_node);
                }
                game_node.parent().remove_child(game_node);
            }
        }
    } catch (const pugi::xpath_exception &e) {
        doc_.Error("Failed to parse path '" + GetPath() + "': " + e.what());
    }
}

std::vector<XmlOperation> XmlOperation::GetXmlOperations(
    XmlOperationContext doc,
    fs::path game_path,
    std::optional<pugi::xml_object_range<pugi::xml_node_iterator>> nodes)
{
#ifndef _WIN32
    auto stricmp = [](auto a, auto b) { return strcasecmp(a, b); };
#endif

    const auto& mod_name = doc.GetName();

    if (!nodes) {
        auto root = doc.GetRoot();
        if (!root) {
            return {};
        }
        nodes = root.children();
    }

    std::vector<XmlOperation> mod_operations;
    for (pugi::xml_node node : *nodes) {
        if (node.type() == pugi::xml_node_type::node_element) {
            if (node.attribute("Skip")) {
                continue;
            }

            if (stricmp(node.name(), "ModOp") == 0) {
                const auto guid = GetXmlPropString(node, "GUID");
                const auto temp = GetXmlPropString(node, "Template");
                std::vector<std::string> guids;
                if (!temp.empty() && !guid.empty()) {
                    doc.Error("Cannot supply both `Template` and `GUID`", node);
                }
                if (!guid.empty()) {
                    std::vector<std::string> guids = absl::StrSplit(guid, ',');
                    for (auto g : guids) {
                        mod_operations.emplace_back(doc, node, g.data(), "", mod_name, game_path);
                    }   
                } else if (!temp.empty()) {
                    mod_operations.emplace_back(doc, node, "", temp, mod_name, game_path);
                } else {
                    mod_operations.emplace_back(doc, node, "", "", mod_name, game_path);
                }
            } else if (stricmp(node.name(), "Group") == 0) {
                auto group_op = XmlOperation{doc, node, "", "", mod_name, game_path};
                group_op.group_ = GetXmlOperations(doc, game_path, node.children());
                mod_operations.push_back(group_op);
            } else if (stricmp(node.name(), "Include") == 0) {
                const auto file = GetXmlPropString(node, "File");
                fs::path relative_include_path;
                if (file.rfind("/", 0) == 0) {
                    relative_include_path = file.substr(1);
                } else {
                    relative_include_path = (doc.GetPath().parent_path() / file).lexically_normal();
                }
                
                auto group_op = XmlOperation{doc, node, "", "", mod_name, game_path};
                group_op.group_ = GetXmlOperations(doc.OpenInclude(relative_include_path),
                                                   game_path);
                mod_operations.push_back(group_op);
            }
        }
    }

    return mod_operations;
}

std::vector<XmlOperation> XmlOperation::GetXmlOperationsFromFile(fs::path    file_path,
                                                                 std::string mod_name,
                                                                 fs::path    game_path,
                                                                 fs::path    mod_path)
{
    const auto mod_relative_path = file_path.lexically_relative(mod_path);
    return GetXmlOperations(XmlOperationContext{mod_relative_path, mod_path, mod_name}, game_path);
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
            doc_.Debug("Condition not matching {} in {} ({}:{})", condition_, mod_name_,
                        mod_path_.string(), doc_.GetLine(node_));
            return false;
        }
    } catch (const pugi::xpath_exception &e) {
        doc_.Error("Failed to parse condition '" + condition_ + "': " + e.what(), node_);
    }

    return true;
}

std::string XmlOperation::GetPath()
{
    return path_;
}

XmlOperation::Type XmlOperation::GetType() const
{
    return type_;
}
