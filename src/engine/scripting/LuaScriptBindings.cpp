#include "LuaScriptBindings.h"
#include "Log.h"
#include "scene/Entity.h"
#include "scene/Scene.h"

namespace Atlas::Scripting
{
    LuaScriptBindings::LuaScriptBindings(Ref<sol::state> luaState, sol::table *atlasNs)
    {
        this->luaState = luaState;
        this->atlasNs = atlasNs;
    }

    void LuaScriptBindings::GenerateBindings()
    {
        atlasNs->set_function("log", [](std::string msg)
                              { Log::Message(msg); });

        atlasNs->new_usertype<glm::vec3>("Vec3",
                                         "x", &glm::vec3::x,
                                         "y", &glm::vec3::y,
                                         "z", &glm::vec3::z);

        auto entityType = atlasNs->new_usertype<Scene::Entity>("Entity");

        auto transformUserType = atlasNs->new_usertype<TransformComponent>("TransformComponent");
        transformUserType["translate"] = &TransformComponent::Translate;

        entityType.set_function("get_transform_component", [](Scene::Entity &entity) -> TransformComponent *
                                { return entity.TryGetComponent<TransformComponent>(); });
    }
}