#include "BindingRegistry.h"

#include <algorithm>
#include <cctype>

namespace Atlas::Scripting {

    std::mutex BindingRegistry::mutex;
    std::atomic_uint64_t BindingRegistry::revision = 0;
    std::vector<BindingRegistry::Owner> BindingRegistry::owners;
    std::unordered_map<std::string, size_t> BindingRegistry::ownerIndices;
    std::unordered_set<std::string> BindingRegistry::memberKeys;
    std::unordered_map<std::type_index, std::string> BindingRegistry::typeNames;
    std::unordered_map<std::string, std::string> BindingRegistry::descriptions;

    // Bound types are written as \x01<index>\x02 while recording, their Lua names are known once all are bound
    static std::vector<std::type_index> placeholderTypes;
    static std::unordered_map<std::type_index, size_t> placeholderIndices;

    bool BindingRegistry::BeginOwner(const std::string& path, const std::string& kind) {

        std::scoped_lock lock(mutex);
        if (ownerIndices.contains(path))
            return false;

        ownerIndices[path] = owners.size();
        owners.push_back({ .path = path, .kind = kind });
        revision++;
        return true;

    }

    void BindingRegistry::AddMember(const std::string& path, Member member) {

        std::scoped_lock lock(mutex);
        if (!memberKeys.insert(path + "." + member.name).second)
            return;

        auto iter = ownerIndices.find(path);
        if (iter == ownerIndices.end()) {
            iter = ownerIndices.emplace(path, owners.size()).first;
            owners.push_back({ .path = path, .kind = "table" });
        }
        owners[iter->second].members.push_back(std::move(member));
        revision++;

    }

    bool BindingRegistry::IsRecorded(const std::string& path, const std::string& name) {

        std::scoped_lock lock(mutex);
        return memberKeys.contains(path + "." + name);

    }

    void BindingRegistry::AddType(const std::type_index& type, const std::string& path) {

        std::scoped_lock lock(mutex);
        if (typeNames.try_emplace(type, path).second)
            revision++;

    }

    void BindingRegistry::Describe(const std::string& path, const std::string& description) {

        std::scoped_lock lock(mutex);
        descriptions[path] = description;

    }

    std::string BindingRegistry::Placeholder(const std::type_index& type) {

        std::scoped_lock lock(mutex);
        auto [iter, inserted] = placeholderIndices.try_emplace(type, placeholderTypes.size());
        if (inserted)
            placeholderTypes.push_back(type);
        return "\x01" + std::to_string(iter->second) + "\x02";

    }

    std::string BindingRegistry::GetTypeName(const std::type_index& type) {

        std::scoped_lock lock(mutex);
        auto iter = typeNames.find(type);
        return iter != typeNames.end() ? iter->second : std::string();

    }

    std::string BindingRegistry::Resolve(const std::string& type) {

        // Called with the mutex locked
        std::string resolved;
        size_t position = 0;
        while (true) {
            auto start = type.find('\x01', position);
            if (start == std::string::npos)
                break;
            auto end = type.find('\x02', start);
            resolved += type.substr(position, start - position);
            auto index = std::stoul(type.substr(start + 1, end - start - 1));
            auto name = typeNames.find(placeholderTypes[index]);
            resolved += name != typeNames.end() ? name->second : "userdata";
            position = end + 1;
        }
        return resolved + type.substr(position);

    }

    std::vector<BindingRegistry::Owner> BindingRegistry::GetOwners() {

        std::scoped_lock lock(mutex);
        auto result = owners;
        for (auto& owner : result) {
            if (auto description = descriptions.find(owner.path); description != descriptions.end())
                owner.description = description->second;
            for (auto& member : owner.members) {
                member.type = Resolve(member.type);
                if (auto description = descriptions.find(owner.path + "." + member.name); description != descriptions.end())
                    member.description = description->second;
            }
        }
        return result;

    }

    json BindingRegistry::ToJson(const std::string& filter) {

        auto lower = [](std::string text) {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            return text;
            };
        auto query = lower(filter);

        json result = json::array();
        for (const auto& owner : GetOwners()) {
            auto ownerMatches = query.empty() || lower(owner.path).find(query) != std::string::npos;
            json members = json::array();
            for (const auto& member : owner.members) {
                if (!ownerMatches && lower(member.name).find(query) == std::string::npos)
                    continue;
                json entry = { { "name", member.name }, { "kind", member.kind }, { "type", member.type } };
                if (!member.description.empty())
                    entry["description"] = member.description;
                members.push_back(entry);
            }
            if (!ownerMatches && members.empty())
                continue;

            json entry = { { "path", owner.path }, { "kind", owner.kind }, { "members", members } };
            if (!owner.description.empty())
                entry["description"] = owner.description;
            result.push_back(entry);
        }
        return result;

    }

    // The language server needs names for parameters, e.g. fun(number): Vec3 -> fun(p1: number): Vec3, methods get self
    static std::string ToLanguageServerType(const std::string& type, const std::string& self) {

        std::string result;
        size_t position = 0;
        while (true) {
            auto start = type.find("fun(", position);
            if (start == std::string::npos)
                break;
            result += type.substr(position, start - position) + "fun(";

            // The parameters end at the matching parenthesis, types like fun(...) can be nested
            auto depth = 1;
            auto end = start + 4;
            for (; end < type.size() && depth > 0; end++)
                depth += type[end] == '(' ? 1 : type[end] == ')' ? -1 : 0;
            auto parameters = type.substr(start + 4, end - start - 5);

            std::vector<std::string> named;
            if (!self.empty())
                named.push_back("self: " + self);
            size_t begin = 0, index = 1;
            depth = 0;
            for (size_t i = 0; i <= parameters.size(); i++) {
                if (i < parameters.size() && parameters[i] == '(') depth++;
                if (i < parameters.size() && parameters[i] == ')') depth--;
                if (i == parameters.size() || (parameters[i] == ',' && depth == 0)) {
                    auto parameter = parameters.substr(begin, i - begin);
                    parameter.erase(0, parameter.find_first_not_of(' '));
                    if (!parameter.empty())
                        named.push_back(parameter == "..." ? "..." : "p" + std::to_string(index++) + ": " + parameter);
                    begin = i + 1;
                }
            }
            for (size_t i = 0; i < named.size(); i++)
                result += (i > 0 ? ", " : "") + named[i];
            result += ")";
            position = end;
        }
        return result + type.substr(position);

    }

    std::string BindingRegistry::ToLuaAnnotations() {

        std::string text = "---@meta\n-- Generated from the bindings of the Atlas Engine, don't edit\n\n";
        auto owners = GetOwners();

        // Tables like Atlas exist before their types are put into them
        std::vector<std::string> roots;
        for (const auto& owner : owners) {
            auto root = owner.path.substr(0, owner.path.find('.'));
            if (root != "_G" && root != owner.path && std::find(roots.begin(), roots.end(), root) == roots.end())
                roots.push_back(root);
        }
        for (const auto& root : roots)
            text += root + " = {}\n";
        text += "\n";

        for (const auto& owner : owners) {
            // Functions of the script environment are globals
            if (owner.path == "_G") {
                for (const auto& member : owner.members)
                    text += "---@type " + ToLanguageServerType(member.type, "") + "\n" + member.name + " = nil\n\n";
                continue;
            }

            if (!owner.description.empty())
                text += "--- " + owner.description + "\n";
            text += "---@class " + owner.path + "\n";
            for (const auto& member : owner.members) {
                if (member.kind == "constructor" && member.name != "new")
                    continue;
                auto type = member.kind == "value" ? owner.path :
                    ToLanguageServerType(member.type, member.kind == "method" ? owner.path : "");
                text += "---@field " + member.name + " " + type;
                text += member.description.empty() ? "\n" : " " + member.description + "\n";
            }
            text += owner.path + " = {}\n\n";
        }
        return text;

    }

}
