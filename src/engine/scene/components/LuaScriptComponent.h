#pragma once

#include "../../System.h"
#include "../../scripting/Script.h"
#include "../../resource/Resource.h"
#include "../Entity.h"
#include "scripting/LuaScriptManager.h"

#include <sol/sol.hpp>
#include <map>

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
                Boolean,
                Vec2,
                Vec3,
                Vec4
            };

            class ScriptProperty {
            public:
                ScriptProperty() = default;

                template<class T>
                void SetValue(const T value);

                template<class T>
                T GetValue() const;

                PropertyType type = PropertyType::Undefined;

                std::string stringValue = "";
                double doubleValue = 0.0;
                int integerValue = 0;
                bool booleanValue = false;

                vec2 vec2Value = vec2(0.0f);
                vec3 vec3Value = vec3(0.0f);
                vec4 vec4Value = vec4(0.0f);

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

            std::map<std::string, ScriptProperty> properties;

            bool permanentExecution = false;

        protected:
            void Update(Scripting::LuaScriptManager& scriptManager, float deltaTime);

        private:
            Scene* scene = nullptr;
            Entity entity = Entity();
            Scripting::LuaScriptManager* scriptManager = nullptr;

            // Force initial execution to be recognized as a changed script
            bool scriptWasModifiedInLastUpdate = true;
            bool environmentNeedsInitialization = true;

            std::optional<sol::protected_function> updateFunction;
            std::optional<sol::environment> scriptEnvironment;

            bool InitScriptEnvironment();
            std::map<std::string, ScriptProperty> GetPropertiesFromScript();
            void GetOrUpdatePropertiesFromScript(bool update);
            void SetPropertyValuesInLuaState();
        };

        template<class T>
        void LuaScriptComponent::ScriptProperty::SetValue(const T value) {

            static_assert(std::is_same_v<T, std::string> || std::is_same_v<T, double> || 
                std::is_same_v<T, int32_t> || std::is_same_v<T, bool> || std::is_same_v<T, vec2> ||
                std::is_same_v<T, vec3> || std::is_same_v<T, vec4>, "Unsupported type");

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
            else if constexpr (std::is_same_v<T, vec2>) {
                vec2Value = value;
                type = PropertyType::Vec2;
            }
            else if constexpr (std::is_same_v<T, vec3>) {
                vec3Value = value;
                type = PropertyType::Vec3;
            }
            else if constexpr (std::is_same_v<T, vec4>) {
                vec4Value = value;
                type = PropertyType::Vec4;
            }

            wasChanged = true;
        }

        template<class T>
        T LuaScriptComponent::ScriptProperty::GetValue() const {

            AE_ASSERT(type != PropertyType::Undefined && "This property was most likely not defined properly");

            static_assert(std::is_same_v<T, std::string> || std::is_same_v<T, double> ||
                std::is_same_v<T, int32_t> || std::is_same_v<T, bool> || std::is_same_v<T, vec2> ||
                std::is_same_v<T, vec3> || std::is_same_v<T, vec4>, "Unsupported type");

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
            else if constexpr (std::is_same_v<T, vec2>) {
                return vec2Value;
            }
            else if constexpr (std::is_same_v<T, vec3>) {
                return vec3Value;
            }
            else if constexpr (std::is_same_v<T, vec4>) {
                return vec4Value;
            }
            else {
                
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