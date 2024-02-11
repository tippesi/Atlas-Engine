#pragma once

#include "../../System.h"
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
            LuaScriptComponent(const LuaScriptComponent &that) = default;

            std::string code;

        protected:
            void Update(float deltaTime);

        private:
            Scene *scene;
            Entity entity;

            Ref<sol::state> luaState;

            void InitLuaState();
        };

    }

}