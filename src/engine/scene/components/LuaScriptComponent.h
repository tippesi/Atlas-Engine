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

                double doubleValue = 0.0;
                int integerValue = 0;
                std::string stringValue = "";
                bool booleanValue = false;
            };

            LuaScriptComponent(Scene *scene, Entity entity);
            LuaScriptComponent(Scene *scene, Entity entity, const LuaScriptComponent &that);
            LuaScriptComponent() = default;

            ResourceHandle<Scripting::Script> script;
            std::vector<ScriptProperty> properties;

            bool permanentExecution = false;

        protected:
            void Update(Scripting::LuaScriptManager& scriptManager, float deltaTime);

        private:
            Scene *scene;
            Entity entity;
            Scripting::LuaScriptManager* scriptManager = nullptr;
            
            // Force initial execution to be recognized as a changed script
            bool scriptWasModifiedInLastUpdate = true;

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