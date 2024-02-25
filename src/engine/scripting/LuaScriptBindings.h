#pragma once

#include "LuaScriptManager.h"

namespace Atlas::Scripting
{
    class LuaScriptBindings
    {
    public:
        LuaScriptBindings(Ref<sol::state> luaState, sol::table *atlasNs);

        void GenerateBindings();

    private:
        Ref<sol::state> luaState;
        sol::table *atlasNs;
    };
}