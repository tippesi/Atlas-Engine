#pragma once

#include "../LuaScriptManager.h"

namespace Atlas::Scripting::Bindings {

    namespace Detail {

        template<class> struct MemberPointerTraits;

        template<class C, class T>
        struct MemberPointerTraits<T C::*> {
            using Owner = C;
            using Value = T;
        };

    }

    // Binds a class-type member (e.g. a glm vector inside a value type) by reference, like sol2 does by default,
    // but keeps the owning userdata alive as long as the returned reference is reachable. Without this, accessing
    // a member of a temporary (e.g. transform:Decompose().translation) returns a reference into memory that the
    // garbage collector can free and reuse, so later reads return garbage (often NaN).
    template<auto Member>
    auto DependentMember() {

        using Owner = typename Detail::MemberPointerTraits<decltype(Member)>::Owner;
        using Value = typename Detail::MemberPointerTraits<decltype(Member)>::Value;

        return sol::property(
            [](Owner& owner, sol::this_state state) -> sol::object {
                sol::state_view lua(state);
                // Inside the __index metamethod the owning userdata is the first stack slot
                sol::object ownerObject(state, 1);
                sol::object reference = sol::make_object(lua, &(owner.*Member));

                // Weak keys make this an ephemeron table: the owner lives exactly as long as the reference
                sol::table owners = lua.registry()["AtlasMemberOwners"].get_or_create<sol::table>();
                if (!owners[sol::metatable_key].valid())
                    owners[sol::metatable_key] = lua.create_table_with("__mode", "k");
                owners[reference] = ownerObject;

                return reference;
            },
            [](Owner& owner, const Value& value) { owner.*Member = value; });

    }

}
