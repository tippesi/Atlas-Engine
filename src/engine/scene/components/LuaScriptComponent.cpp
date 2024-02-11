#include "LuaScriptComponent.h"

#include "../Scene.h"
#include "TransformComponent.h"

namespace Atlas::Scene::Components
{

    LuaScriptComponent::LuaScriptComponent(Scene *scene, Entity entity) : scene(scene), entity(entity)
    {
        // TODO: implement
    }

    LuaScriptComponent::LuaScriptComponent(Scene *scene, Entity entity, const LuaScriptComponent &that) : scene(scene), entity(entity), script(that.script)
    {
    }

    void LuaScriptComponent::Update(float deltaTime)
    {
        AE_ASSERT(scene != nullptr);
        AE_ASSERT(scene->physicsWorld != nullptr);

        if (!script.IsLoaded())
            return;

        auto resource = script.GetResource();
        if (resource->WasModified())
        {
            // Do reload here, adjust modified time beforehand to be conservative
            resource->UpdateModifiedTime();
            script->Reload();
            // Proceed to do more here.... in case we need it
        }

        if (scene->physicsWorld->pauseSimulation)
        {
            // the instance is not running, discard the state
            scriptUpdate.reset();
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
        if (scriptUpdate.has_value())
        {
            auto result = scriptUpdate.value()(deltaTime);
            if (!result.valid())
            {
                // Call failed
                // Note that if the handler was successfully called, this will include
                // the additional appended error message information of
                // "got_problems handler: " ...
                sol::error err = result;
                std::string what = err.what();
                Log::Error("Error while executing update: " + what);
            }
        }
    }

    void LuaScriptComponent::InitLuaState()
    {
        AE_ASSERT(luaState == nullptr);
        try
        {
            // create lua state
            luaState = std::make_shared<sol::state>();
            luaState->open_libraries(sol::lib::base);
            auto &state = *luaState;

            // create bindings for script
            auto ns = state["Atlas"].get_or_create<sol::table>();

            ns.set_function("log", &Log::Message);

            ns.new_usertype<glm::vec3>("Vec3",
                                       "x", &glm::vec3::x,
                                       "y", &glm::vec3::y,
                                       "z", &glm::vec3::z);

            auto entityType = ns.new_usertype<Entity>("Entity");
            ns.set_function("get_this_entity", [&]()
                            { return this->entity; });

            auto transformUserType = ns.new_usertype<TransformComponent>("TransformComponent");
            transformUserType["translate"] = &TransformComponent::Translate;

            entityType.set_function("get_transform_component", [](Entity &entity) -> TransformComponent *
                                    { return entity.TryGetComponent<TransformComponent>(); });

            // load script
            state.script(script->code);

            // now load the script functions

            sol::protected_function updateFunction = state["update"];
            if (updateFunction.valid())
            {
                scriptUpdate = updateFunction;
            }
        }
        catch (const std::exception &e)
        {
            luaState = nullptr;
            Atlas::Log::Message("Error while compiling lua script: " + std::string(e.what()));
        }
    }
}