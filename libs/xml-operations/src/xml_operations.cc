#include "xml_operations.h"

#include "spdlog/spdlog.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>

std::vector<std::string> StrSplit(const std::string& input, char delimiter) {
    std::vector<std::string> result;

    int last_pos = 0;
    for (int i = 0; i < input.length(); i++) {
        if (input[i] != delimiter) {
            continue;
        }

        if (i - last_pos > 0) {
            result.emplace_back(input.substr(last_pos, i - last_pos));
        }
        last_pos = i + 1;
    }

    if (last_pos != input.length()) {
        result.emplace_back(input.substr(last_pos, input.length() - last_pos));
    }
    
    return result;
}

XmlOperationContext::XmlOperationContext() { }

XmlOperationContext::XmlOperationContext(const fs::path& mod_relative_path,
                                         const fs::path& mod_base_path,
                                         std::string mod_name)
{
    if (mod_name.empty()) {
        mod_name = mod_base_path.filename().string();
    }

    include_loader_ = [this, &mod_base_path, &mod_name](const fs::path& file_path) {
        std::vector<char> buffer;
        size_t size;
        if (!ReadFile(mod_base_path / file_path, buffer, size)) {
            spdlog::error("{}: Failed to open {}",
                          mod_name,
                          file_path.string());
            return std::make_shared<XmlOperationContext>();
        }
        return std::make_shared<XmlOperationContext>(buffer.data(), size, file_path, mod_name, *this->include_loader_);
    };

    *this = *(*include_loader_)(mod_relative_path);
    mod_name_ = mod_name;
}

XmlOperationContext::XmlOperationContext(const char* buffer, size_t size,
                                         const fs::path& doc_path,
                                         const std::string& mod_name,
                                         std::optional<include_loader_t> include_loader)
{
    mod_name_ = mod_name;
    include_loader_ = include_loader;
    doc_path_ = doc_path.generic_string();

    offset_data_ = BuildOffsetData(buffer, size);
    doc_ = std::make_shared<pugi::xml_document>();
    auto parse_result = doc_->load_buffer(buffer, size);
    if (!parse_result) {
        spdlog::error("{}: Failed to parse {} (line {}): {}",
                      mod_name, doc_path_,
                      this->GetLine(parse_result.offset), parse_result.description());
    }
}

std::shared_ptr<XmlOperationContext> XmlOperationContext::OpenInclude(const fs::path& file_path) const
{
    if (!include_loader_) {
        return {};
    }

    auto include = (*include_loader_)(file_path);
    include->mod_name_ = mod_name_;
    include->include_loader_ = this->include_loader_;
    return include;
}

size_t XmlOperationContext::GetLine(ptrdiff_t offset) const
{
    auto it = std::lower_bound(offset_data_.begin(), offset_data_.end(), offset);
    return (it - offset_data_.begin()) + 1;
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

void XmlOperationContext::Debug(std::string_view msg, pugi::xml_node node) const
{
    spdlog::debug("{}: {} ({}:{})", mod_name_, msg, doc_path_, node ? GetLine(node) : 0);
}

void XmlOperationContext::Warn(std::string_view msg, pugi::xml_node node) const
{
    spdlog::warn("{}: {} ({}:{})", mod_name_, msg, doc_path_, node ? GetLine(node) : 0);
}

void XmlOperationContext::Error(std::string_view msg, pugi::xml_node node) const
{
    spdlog::error("{}: {} ({}:{})", mod_name_, msg, doc_path_, node ? GetLine(node) : 0);
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

XmlLookup::XmlLookup() { }

XmlLookup::XmlLookup(const std::string& path,
                     const std::string& guid,
                     const std::string& templ,
                     bool explicit_speculative,
                     std::shared_ptr<XmlOperationContext> context,
                     pugi::xml_node node)
{
    context_ = context;
    node_ = node;

    empty_path_ = path.empty();
    negative_ = !path.empty() && path[0] == '!';

    std::string read_path = negative_ ? path.substr(1) : path;

    if (!read_path.empty() && read_path[0] == '~') {
        read_path = read_path.substr(1);
        guid_ = guid;
        template_ = templ;
    }
    else if (explicit_speculative) {
        guid_ = {};
        template_ = {};
    }
    else {
        guid_ = guid;
        template_ = templ;
    }

    ReadPath(read_path, guid_, template_);
}

pugi::xpath_node_set XmlLookup::Select(std::shared_ptr<pugi::xml_document> doc, std::optional<pugi::xml_node>* assetNode, bool strict) const
{
    try {
        auto results = ReadGuidNodes(doc, assetNode);
        if (!results.empty() || (strict && !guid_.empty())) {
            return results;
        }

        results = ReadTemplateNodes(doc);
        if (!results.empty() || (strict && !template_.empty())) {
            return results;
        }

        if (!guid_.empty() || !template_.empty()) {
            context_->Debug("Speculative path failed to find node with path {} {}", path_, speculative_path_);
        }
        return doc->select_nodes(path_.c_str());
    } catch (const pugi::xpath_exception &e) {
        context_->Error("Failed to parse path '" + path_ + "': " + e.what(), node_);
    }

    return {};
}

XmlOperation::XmlOperation(std::shared_ptr<XmlOperationContext> doc, pugi::xml_node node,
                           const std::string& guid, const std::string& templ) : doc_(doc)
{
    node_     = node;

    path_ = XmlLookup{GetXmlPropString(node, "Path"), guid, templ, false, doc, node};
    ReadType(node);
    if (type_ != Type::Remove) {
        nodes_ = node.children();
    }

    condition_ = XmlLookup{node.attribute("Condition").as_string(), guid, templ, true, doc, node};
    allow_no_match_ = node.attribute("AllowNoMatch");

    if (type_ != Type::Remove) {
        content_ = XmlLookup{node.attribute("Content").as_string(), guid, templ, true, doc, node};
        if (!content_.IsEmpty() && nodes_->begin() != nodes_->end()) {
            doc_->Error("ModOp must be empty when Content is used", node_);
            nodes_ = {};
        }
    }
}

void XmlLookup::ReadPath(std::string prop_path, std::string guid, std::string temp)
{
    if (prop_path.find('&') != std::string::npos)
    {
        prop_path = std::regex_replace(prop_path, std::regex{"&gt;"}, ">");
        prop_path = std::regex_replace(prop_path, std::regex{"&lt;"}, "<");
    }

    if (guid.empty()) {
        // Rewrite path to use faster GUID lookup
        int g;
        // Matches stuff like this and extracts GUID //Assets[Asset/Values/Standard/GUID='102119']
        if (sscanf(prop_path.c_str(), "//Assets[Asset/Values/Standard/GUID='%d']", &g) > 0) {
            if (std::string("//Assets[Asset/Values/Standard/GUID='") + std::to_string(g) + "']"
                == prop_path) {
                guid                   = std::to_string(g);
                guid_                  = std::to_string(g);
                speculative_path_type_ = SpeculativePathType::ASSET_CONTAINER;
            }
        }
        else if (sscanf(prop_path.c_str(), "//Asset/Values[Standard/GUID='%d']", &g) > 0) {
            const auto match = std::string("//Asset/Values[Standard/GUID='") + std::to_string(g) + "']";
            if (prop_path.rfind(match, 0) == 0) {
                guid = std::to_string(g);
                guid_ = std::to_string(g);
                speculative_path_type_ = SpeculativePathType::VALUES_CONTAINER;
                prop_path = prop_path.substr(match.length());
                path_ = "//Asset/Values[Standard/GUID='" + guid + "']";
            }
            else {
                context_->Warn("Failed to construct speculative path lookup: '" + prop_path + "'", node_);
            }
        }
        else if (sscanf(prop_path.c_str(), "//Values[Standard/GUID='%d']", &g) > 0) {
            const auto match = std::string("//Values[Standard/GUID='") + std::to_string(g) + "']";
            if (prop_path.rfind(match, 0) == 0) {
                guid = std::to_string(g);
                guid_ = std::to_string(g);
                speculative_path_type_ = SpeculativePathType::VALUES_CONTAINER;
                prop_path = prop_path.substr(match.length());
                path_ = "//Values[Standard/GUID='" + guid + "']";
            }
            else {
                context_->Warn("Failed to construct speculative path lookup: '" + prop_path + "'", node_);
            }
        }
    } else {
        speculative_path_type_ = SpeculativePathType::SINGLE_ASSET;
        path_                  = "//Asset[Values/Standard/GUID='" + guid + "']";
    }

    if (prop_path.empty()) {
        prop_path = "/";
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

void XmlOperation::ReadType(pugi::xml_node node)
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
        doc_->Error("Unknown ModOp " + type, node_);
    }
}

std::optional<pugi::xml_node> XmlLookup::FindAsset(const std::string& guid, pugi::xml_node node, int speculate_position) const
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
        } else if (speculative_path_type_ == SpeculativePathType::VALUES_CONTAINER) {
            return values;
        } else {
            return node;
        }
    }

    if (speculate_position == 0) {
        // first group level
        static std::optional<pugi::xml_node> last_search;
        if (last_search) {
            for (pugi::xml_node n : node.children()) {
                if (n == last_search) {
                    if (auto found = FindAsset(guid, n, speculate_position - 1); found) {
                        last_search = n;
                        return found;
                    }
                }
            }
        }
        for (pugi::xml_node n : node.children()) {
            if (n != last_search) {
                if (auto found = FindAsset(guid, n, speculate_position - 1); found) {
                    last_search = n;
                    return found;
                }
            }
        }
    }
    else {
        // normal asset finding
        for (pugi::xml_node n : node.children()) {
            if (auto found = FindAsset(guid, n, speculate_position - 1); found) {
                return found;
            }
        }
    }

    return {};
}

std::optional<pugi::xml_node> XmlLookup::FindTemplate(const std::string& temp, pugi::xml_node node) const
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

std::optional<pugi::xml_node> XmlLookup::FindTemplate(std::shared_ptr<pugi::xml_document> doc,
                                                      const std::string& temp) const
{
    return FindTemplate(temp, doc->root());
}

pugi::xpath_node_set XmlLookup::ReadGuidNodes(std::shared_ptr<pugi::xml_document> doc, std::optional<pugi::xml_node>* assetNode) const
{
    pugi::xpath_node_set results;
    std::optional<pugi::xml_node> node;

    if (!guid_.empty()) {
        try {
            auto cached = (assetNode && *assetNode) ? FindAsset(guid_, **assetNode) : std::optional<pugi::xml_node>{};
            node = cached ? cached : FindAsset(guid_, doc->root());
            if (node) {
                if (speculative_path_ != "*") {
                    results = node->select_nodes(speculative_path_.c_str());
                }
            }
        } catch (const pugi::xpath_exception& e) {
            context_->Error("Speculative path failed to find node with path '" + GetPath() +
                       "' " + speculative_path_);
            context_->Error(e.what());
        }
    }

    if (assetNode) {
        *assetNode = node;
    }
    return results;
}

pugi::xpath_node_set XmlLookup::ReadTemplateNodes(std::shared_ptr<pugi::xml_document> doc) const
{
    pugi::xpath_node_set results;
    if (!template_.empty()) {
        try {
            auto node = FindTemplate(doc, template_);
            if (node && speculative_path_ != "*") {
                results = node->select_nodes(speculative_path_.c_str());
            }
        } catch (const pugi::xpath_exception& e) {
            context_->Error("Speculative path failed to find node with path '" + GetPath() +
                       "' " + speculative_path_);
            context_->Error(e.what());
        }
    }
    return results;
}

void XmlOperation::Apply(std::shared_ptr<pugi::xml_document> doc)
{
    auto start = std::chrono::high_resolution_clock::now();
    auto logTime = [&start, this](const char* group = "ModOp") {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        this->doc_->Debug("Time: {}ms {} ({}:{})", duration, group,
            this->doc_->GetGenericPath(), this->doc_->GetLine(node_));
    };

    std::optional<pugi::xml_node> cachedNode;
    if (GetType() == XmlOperation::Type::None || !CheckCondition(doc, cachedNode)) {
        return logTime(type_ == Type::Group ? "Group" : "ModOp");
    }

    if (type_ == Type::Group) {
        // logTime();
        for (auto& modop : group_) {
            modop.Apply(doc);
        }
        logTime("Group");
        return;
    }

    std::vector<pugi::xml_node> content_nodes;
    if (type_ != Type::Remove && !content_.IsEmpty()) {
        pugi::xpath_node_set result = content_.Select(doc);
        if (result.empty()) {
            doc_->Warn("No matching node for path " + path_.GetPath() , node_);
            return logTime();
        }
        for (auto& node : result)
            content_nodes.push_back(node.node());
    }
    if (content_.IsEmpty() && nodes_) {
        content_nodes.insert(content_nodes.end(), nodes_->begin(), nodes_->end());
    }

    try {
        doc_->Debug("Looking up {}", path_.GetPath());
        auto results = path_.Select(doc, &cachedNode);
        if (results.empty()) {
            if (allow_no_match_) {
                doc_->Debug("No matching node for Path '{}'", path_.GetPath());
            }
            else {
                doc_->Warn("No matching node for Path '" + path_.GetPath() + "'", node_);
            }
            return logTime();
        }

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
        doc_->Error("Failed to parse path '" + path_.GetPath() + "': " + e.what());
    }

    logTime();
}

std::vector<XmlOperation> XmlOperation::GetXmlOperations(
    std::shared_ptr<XmlOperationContext> doc,
    const fs::path& game_path,
    std::optional<pugi::xml_object_range<pugi::xml_node_iterator>> nodes)
{
#ifndef _WIN32
    auto stricmp = [](auto a, auto b) { return strcasecmp(a, b); };
#endif

    if (!doc) {
        return {};
    }

    if (!nodes) {
        auto root = doc->GetRoot();
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
                    doc->Error("Cannot supply both `Template` and `GUID`", node);
                }
                if (!guid.empty()) {
                    std::vector<std::string> guids = StrSplit(guid, ',');
                    for (auto g : guids) {
                        mod_operations.emplace_back(doc, node, g.data(), "");
                    }
                }
                else {
                    mod_operations.emplace_back(doc, node, "", temp);
                }
            }
            else if (stricmp(node.name(), "Group") == 0) {
                auto group_op = XmlOperation{doc, node};
                group_op.group_ = GetXmlOperations(doc, game_path, node.children());
                mod_operations.push_back(group_op);
            }
            else if (stricmp(node.name(), "Include") == 0) {
                const auto file = GetXmlPropString(node, "File");
                fs::path relative_include_path;
                if (file.rfind("/", 0) == 0) {
                    relative_include_path = file.substr(1);
                } else {
                    relative_include_path = (doc->GetPath().parent_path() / file).lexically_normal();
                }

                auto group_op = XmlOperation{doc, node};
                const auto include_context = doc->OpenInclude(relative_include_path);
                if (include_context->GetGenericPath().empty()) {
                    doc->Error("Include file missing or empty: " + relative_include_path.string(), node);
                }
                else {
                    group_op.group_ = GetXmlOperations(include_context, game_path);
                    mod_operations.push_back(group_op);
                }
            }
        }
    }

    return mod_operations;
}

std::vector<XmlOperation> XmlOperation::GetXmlOperationsFromFile(const fs::path&    file_path,
                                                                 std::string        mod_name,
                                                                 const fs::path&    game_path,
                                                                 const fs::path&    mod_path)
{
    const auto mod_relative_path = file_path.lexically_relative(mod_path);
    return GetXmlOperations(std::make_shared<XmlOperationContext>(mod_relative_path, mod_path, mod_name), game_path);
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

bool XmlOperation::CheckCondition(std::shared_ptr<pugi::xml_document> doc, std::optional<pugi::xml_node>& cachedNode)
{
    if (condition_.IsEmpty()) {
        return true;
    }

    const auto match_nodes = condition_.Select(doc, &cachedNode, true);

    if (condition_.IsNegative() != match_nodes.empty()) {
        doc_->Debug("Condition not matching {} in {} ({}:{})", condition_.GetPath(), doc_->GetName(),
                   doc_->GetGenericPath(), doc_->GetLine(node_));
        return false;
    }

    return true;
}

XmlOperation::Type XmlOperation::GetType() const
{
    return type_;
}
