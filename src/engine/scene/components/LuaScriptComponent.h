#pragma once

#include "../../System.h"
#include "../../scripting/Script.h"
#include "../../resource/Resource.h"
#include "../Entity.h"

#include <sol/sol.hpp>

namespace Atlas::Scene
{

    class Scene;

    namespace Components
    {

        class LuaScriptComponent
        {
            friend Scene;

        public:
            LuaScriptComponent(Scene *scene, Entity entity);
            LuaScriptComponent(Scene *scene, Entity entity, const LuaScriptComponent& that);
            LuaScriptComponent() = default;

            ResourceHandle<Scripting::Script> script;

        protected:
            void Update(float deltaTime);

        private:
            Scene *scene;
            Entity entity;

            Ref<sol::state> luaState;
            void InitLuaState();

            std::optional<sol::protected_function> scriptUpdate;
        };

    }

}