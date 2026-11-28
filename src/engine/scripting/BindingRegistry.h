#pragma once

#include "../System.h"
#include "../common/SerializationHelper.h"

#include <atomic>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Atlas::Scripting {

    // What the Lua bindings offer: types with their fields, functions and constructors and the functions of tables like
    // Host or Bridge. Binder records it while the bindings are generated, each owner once per process. The types of
    // the signatures come from the C++ types at compile time, names of bound types are resolved when the registry is
    // read, e.g. by the MCP tool bindings, the code completion of the editor or the export for the Lua language server
    class BindingRegistry {

    public:
        struct Member {
            std::string name;
            // field, property, function, method, constructor, value
            std::string kind;
            // Lua types, e.g. "number", "fun(Vec3, number?): Entity", bound types still as placeholders
            std::string type;
            std::string description;
        };

        struct Owner {
            // e.g. Atlas.Sky, Host, Glm
            std::string path;
            // type, table, enum
            std::string kind;
            std::vector<Member> members;
            std::string description;
        };

        // Returns false if the owner was recorded before, its members aren't recorded again then
        static bool BeginOwner(const std::string& path, const std::string& kind);

        // Members of owners which are extended later, e.g. Host by a game, are recorded once per name
        static void AddMember(const std::string& path, Member member);

        // Whether a member was recorded already, the type strings are only built the first time
        static bool IsRecorded(const std::string& path, const std::string& name);

        // Counts up with every recorded owner, member or type, e.g. for caches of completions
        static uint64_t GetRevision() { return revision; }

        static void AddType(const std::type_index& type, const std::string& path);

        static void Describe(const std::string& path, const std::string& description);

        // The owners whose path or member names contain the filter, all without one, as json with resolved types
        static json ToJson(const std::string& filter = "");

        // Annotations for the Lua language server (---@class, ---@field), e.g. for the code completion of other editors
        static std::string ToLuaAnnotations();

        // Owners and their members with resolved type names
        static std::vector<Owner> GetOwners();

        // The Lua name of a bound type, empty if it isn't bound
        static std::string GetTypeName(const std::type_index& type);

        static std::string Placeholder(const std::type_index& type);

    private:
        static std::string Resolve(const std::string& type);

        static std::mutex mutex;
        static std::atomic_uint64_t revision;
        static std::vector<Owner> owners;
        static std::unordered_map<std::string, size_t> ownerIndices;
        static std::unordered_set<std::string> memberKeys;
        static std::unordered_map<std::type_index, std::string> typeNames;
        static std::unordered_map<std::string, std::string> descriptions;

    };

}
