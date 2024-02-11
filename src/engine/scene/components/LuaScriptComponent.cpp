#include "LuaScriptComponent.h"

#include "../Scene.h"

namespace Atlas::Scene::Components
{

    LuaScriptComponent::LuaScriptComponent(Scene *scene, Entity entity) : scene(scene), entity(entity)
    {
        // TODO: implement
    }

    void LuaScriptComponent::Update(float deltaTime)
    {
        AE_ASSERT(scene != nullptr);
        AE_ASSERT(scene->physicsWorld != nullptr);

        if (!script.IsLoaded())
            return;

        auto resource = script.GetResource();
        if (resource->WasModified()) {
            // Do reload here, adjust modified time beforehand to be conservative
            resource->UpdateModifiedTime();
            script->Reload();
            // Proceed to do more here.... in case we need it
        }

        if (scene->physicsWorld->pauseSimulation)
        {
            // the instance is not running, discard the state
            luaState = nullptr;
            return;
        }

        // the instance is running, create a state if not existing
        if (luaState == nullptr)
        {
            InitLuaState();
        }

        // call the script function
        AE_ASSERT(luaState != nullptr);
        auto &state = *luaState;
        sol::protected_function updateFunction = state["update"];
        AE_ASSERT(updateFunction.valid());
        std::function<void(double)> stdUpdateFunction = updateFunction;
        stdUpdateFunction(deltaTime);
    }

    void LuaScriptComponent::InitLuaState()
    {
        AE_ASSERT(luaState == nullptr);
        luaState = std::make_shared<sol::state>();
        luaState->open_libraries(sol::lib::base);
        luaState->set_function("log", [&](std::string msg)
                               { Log::Message(msg); });

        luaState->script(script->code);
    }
}