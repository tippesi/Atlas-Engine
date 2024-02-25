#pragma once

#include "../../System.h"
#include "../../scripting/Script.h"
#include "../../resource/Resource.h"
#include "../Entity.h"
#include "scripting/LuaScriptManager.h"

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
            enum class PropertyType
            {
                String,
                Double,
                Integer,
                Boolean
            };

            struct ScriptProperty
            {
                std::string name;
                PropertyType type;

                double doubleValue;
                int integerValue;
                std::string stringValue;
                bool booleanValue;
            };

            LuaScriptComponent(Scene *scene, Entity entity);
            LuaScriptComponent(Scene *scene, Entity entity, const LuaScriptComponent &that);
            LuaScriptComponent() = default;

            ResourceHandle<Scripting::Script> script;
            std::vector<ScriptProperty> properties;

        protected:
            void Update(Scripting::LuaScriptManager& scriptManager, float deltaTime);

        private:
            Scene *scene;
            Entity entity;
            Scripting::LuaScriptManager* scriptManager = nullptr;
            
            bool scriptWasModifiedInLastUpdate = false;

            std::optional<sol::protected_function> updateFunction;
            std::optional<sol::environment> scriptEnvironment;
            
            bool InitScriptEnvironment();
            std::vector<ScriptProperty> GetPropertiesFromScript();
            void GetOrUpdatePropertiesFromScript();
            void LoadScriptAndFetchProperties();
            void SetPropertyValuesInLuaState();
        };

    }

}