#pragma once

#include "../System.h"
#include "../scene/Entity.h"
#include "../common/Ref.h"

#include <sol/sol.hpp>

namespace Atlas::Scripting
{
    class LuaScriptManager
    {
    public:
        LuaScriptManager(Scene::Scene* scene);     

        sol::state& state();   

    private:
        Ref<sol::state> luaState;
        Scene::Scene* scene;

        void InitLuaState();
    };
}
