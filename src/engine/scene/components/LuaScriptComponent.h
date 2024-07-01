#pragma once

#include "../../System.h"
#include "../../scripting/Script.h"
#include "../../resource/Resource.h"
#include "../Entity.h"
#include "scripting/LuaScriptManager.h"

#include <sol/sol.hpp>

namespace Atlas::Scene {

    class Scene;

    namespace Components {

        class LuaScriptComponent {
            friend Scene;

        public:
            enum class PropertyType {
                Undefined,
                String,
                Double,
                Integer,
                Boolean
            };

            class ScriptProperty {
            public:
                ScriptProperty() = default;

                template<class T>
                void SetValue(const T value);

                template<class T>
                T GetValue() const;

                std::string name;
                PropertyType type = PropertyType::Undefined;

                std::string stringValue = "";
                double doubleValue = 0.0;
                int integerValue = 0;
                bool booleanValue = false;

                bool wasChanged = false;

            };

            LuaScriptComponent() = default;
            explicit LuaScriptComponent(Scene* scene, Entity entity);
            explicit LuaScriptComponent(Scene* scene, Entity entity, const LuaScriptComponent& that);
            explicit LuaScriptComponent(Scene* scene, Entity entity, const ResourceHandle<Scripting::Script>& script);

            void ChangeResource(const ResourceHandle<Scripting::Script>& script);

            bool HasProperty(const std::string& name) const;

            PropertyType GetPropertyType(const std::string& name) const;

            template<class T>
            void SetPropertyValue(const std::string& name, const T value);

            template<class T>
            T GetPropertyValue(const std::string& name) const;

            ResourceHandle<Scripting::Script> script;

            std::unordered_map<std::string, ScriptProperty> properties;

            bool permanentExecution = false;

        protected:
            void Update(Scripting::LuaScriptManager& scriptManager, float deltaTime);

        private:
            Scene* scene = nullptr;
            Entity entity = Entity();
            Scripting::LuaScriptManager* scriptManager = nullptr;

            // Force initial execution to be recognized as a changed script
            bool scriptWasModifiedInLastUpdate = true;

            std::optional<sol::protected_function> updateFunction;
            std::optional<sol::environment> scriptEnvironment;

            bool InitScriptEnvironment();
            std::unordered_map<std::string, ScriptProperty> GetPropertiesFromScript();
            void GetOrUpdatePropertiesFromScript();
            void SetPropertyValuesInLuaState();
        };

        template<class T>
        void LuaScriptComponent::ScriptProperty::SetValue(const T value) {
            if constexpr (std::is_same_v<T, std::string>) {
                stringValue = value;
                type = PropertyType::String;
            }
            else if constexpr (std::is_same_v<T, double>) {
                doubleValue = value;
                type = PropertyType::Double;
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
                integerValue = value;
                type = PropertyType::Integer;
            }
            else if constexpr (std::is_same_v<T, bool>) {
                booleanValue = value;
                type = PropertyType::Boolean;
            }
            else {
                static_assert("Unsupported type" && false);
            }

            wasChanged = true;
        }

        template<class T>
        T LuaScriptComponent::ScriptProperty::GetValue() const {

            AE_ASSERT(type != PropertyType::Undefined && "This property was most likely not defined properly");

            if constexpr (std::is_same_v<T, std::string>) {
                return stringValue;
            }
            else if constexpr (std::is_same_v<T, double>) {
                return doubleValue;
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
                return integerValue;
            }
            else if constexpr (std::is_same_v<T, bool>) {
                return booleanValue;
            }
            else {
                static_assert("Unsupported type" && false);
            }
        }

        template<class T>
        void LuaScriptComponent::SetPropertyValue(const std::string& name, const T value) {

            properties[name].SetValue(value);

        }

        template<class T>
        T LuaScriptComponent::GetPropertyValue(const std::string& name) const {

            ScriptProperty prop{};

            if (properties.contains(name))
                prop = properties.at(name);

            return prop.GetValue<T>();

        }

    }

}