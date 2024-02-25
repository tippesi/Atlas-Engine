#include "LuaScriptManager.h"
#include "Log.h"
#include "LuaScriptBindings.h"

namespace Atlas::Scripting
{
    LuaScriptManager::LuaScriptManager(Scene::Scene *scene)
    {
        this->scene = scene;
        InitLuaState();
    }

    sol::state &LuaScriptManager::state()
    {
        AE_ASSERT(luaState != nullptr);
        return *luaState;
    }

    void LuaScriptManager::InitLuaState()
    {
        AE_ASSERT(luaState == nullptr);

        // create lua state
        luaState = std::make_shared<sol::state>();
        luaState->open_libraries(sol::lib::base);
        auto &state = *luaState;

        // create atlas namespace
        sol::table atlasNs = state["Atlas"].get_or_create<sol::table>();

        // generate bindings
        LuaScriptBindings bindingGenerator(luaState, &atlasNs);
        bindingGenerator.GenerateBindings();
    }
}