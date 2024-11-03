#include "LuaScriptComponent.h"

#include "../Scene.h"
#include "TransformComponent.h"

namespace Atlas::Scene::Components {

    LuaScriptComponent::LuaScriptComponent(Scene* scene, Entity entity) : scene(scene), entity(entity) {



    }

    LuaScriptComponent::LuaScriptComponent(Scene* scene, Entity entity, const LuaScriptComponent& that) {

        if (this != &that) {
            *this = that;
        }

        this->scene = scene;
        this->entity = entity;

        environmentNeedsInitialization = true;

    }

    LuaScriptComponent::LuaScriptComponent(Scene* scene, Entity entity, const ResourceHandle<Scripting::Script>& script)
         : script(script), scene(scene), entity(entity) {

        

    }

    void LuaScriptComponent::ChangeResource(const ResourceHandle<Scripting::Script>& script) {

        this->script = script;

        environmentNeedsInitialization = true;

    }

    bool LuaScriptComponent::HasProperty(const std::string& name) const {

        return properties.contains(name);

    }

    LuaScriptComponent::PropertyType LuaScriptComponent::GetPropertyType(const std::string& name) const {

        ScriptProperty prop{};

        if (properties.contains(name))
            prop = properties.at(name);

        return prop.type;

    }

    void LuaScriptComponent::Update(Scripting::LuaScriptManager& scriptManager, float deltaTime) {
        // set the script manager
        if (this->scriptManager == nullptr) {
            this->scriptManager = &scriptManager;
        }

        AE_ASSERT(scene != nullptr);
        AE_ASSERT(scene->physicsWorld != nullptr);
        AE_ASSERT(this->scriptManager == &scriptManager);

        // check if the script resource is loaded
        if (!script.IsLoaded())
            return;

        // check if script was modified the frame before
        {
            auto resource = script.GetResource();
            if (scriptWasModifiedInLastUpdate && resource->WasModified()) {
                resource->UpdateModifiedTime();
            }
            scriptWasModifiedInLastUpdate = false;
            if (resource->WasModified()) {
                // Do reload here, adjust modified time beforehand to be conservative
                script->Reload();
                scriptWasModifiedInLastUpdate = true;
            }
        }

        if (scriptWasModifiedInLastUpdate || environmentNeedsInitialization) {
            // the script was modified

            // reset the saved references
            updateFunction.reset();
            scriptEnvironment.reset();

            // reload the properties, therefore init the state
            if (!InitScriptEnvironment())
                return;

            // and then get or update the properties from the script
            GetOrUpdatePropertiesFromScript(scriptWasModifiedInLastUpdate);

            environmentNeedsInitialization = false;
        }

        if (scene->physicsWorld->pauseSimulation && !permanentExecution) {
            // the instance is not running, discard the state
            updateFunction.reset();
            scriptEnvironment.reset();
        }
        else {
            // the instance is running, create a state if not existing
            if (!scriptEnvironment.has_value()) {
                if (!InitScriptEnvironment())
                    return;
            }

            // process the script
            AE_ASSERT(scriptEnvironment.has_value());
            SetPropertyValuesInLuaState();

            // call the update function
            try {
                if (updateFunction.has_value()) {
                    auto result = updateFunction.value()(deltaTime);
                    if (!result.valid()) {
                        // Call failed
                        // Note that if the handler was successfully called, this will include
                        // the additional appended error message information of
                        // "got_problems handler: " ...
                        sol::error err = result;
                        std::string what = err.what();
                        Log::Error("Error while executing update in "
                            + script.GetResource()->GetFileName() + ": " + what);
                    }
                }
            }
            catch (const std::exception& e) {
                Atlas::Log::Message("Error while compiling lua script "
                    + script.GetResource()->GetFileName() + ": " + std::string(e.what()));
            }
        }
    }

    bool LuaScriptComponent::InitScriptEnvironment() {
        AE_ASSERT(!scriptEnvironment.has_value());
        try {
            // create environment
            auto& state = scriptManager->state();
            sol::environment scriptEnv(state, sol::create, state.globals());
            scriptEnvironment = scriptEnv;

            // create environment based functions (and copy the necessary lambda captures)
            scriptEnv.set_function("GetThisEntity", [entity = this->entity]() { return entity; });
            scriptEnv.set_function("GetThisScene", [scene = this->scene]() { return scene; });

            // load script
            state.script(script->code, scriptEnv);

            // load the script functions
            sol::protected_function function = scriptEnv["Update"];
            if (function.valid()) {
                this->updateFunction = function;
            }
        }
        catch (const std::exception& e) {
            updateFunction.reset();
            scriptEnvironment.reset();
            Atlas::Log::Message("Error while compiling lua script "
                + script.GetResource()->GetFileName() + ": " + std::string(e.what()));

            return false;
        }

        return true;
    }

    std::map<std::string, LuaScriptComponent::ScriptProperty> LuaScriptComponent::GetPropertiesFromScript() {
        AE_ASSERT(scriptEnvironment.has_value());

        const auto& state = scriptEnvironment.value();

        sol::optional<sol::table> scriptProperties = state["ScriptProperties"];
        if (!scriptProperties.has_value()) {
            return {};
        }

        std::map<std::string, ScriptProperty> foundProperties;
        for (const auto& entry : scriptProperties.value()) {
            ScriptProperty scriptProperty = {};

            // determine property name
            const auto& key = entry.first;
            if (!key.valid())
                continue;
            if (!key.is<std::string>())
                continue;
            const auto name = key.as<std::string>();

            // determine property type and value
            const auto& propertyTable = entry.second;
            if (!propertyTable.valid())
                continue;
            if (!propertyTable.is<sol::table>())
                continue;
            const auto& tbl = propertyTable.as<sol::table>();
            if (!tbl.valid())
                continue;

            // determine property type
            sol::optional<std::string> type = tbl["type"];
            if (!type.has_value())
                continue;

            auto typeValue = type.value();
            std::transform(typeValue.begin(), typeValue.end(), typeValue.begin(), ::tolower);

            if (typeValue == "string") {
                scriptProperty.type = PropertyType::String;
            }
            else if (typeValue == "double") {
                scriptProperty.type = PropertyType::Double;
            }
            else if (typeValue == "integer") {
                scriptProperty.type = PropertyType::Integer;
            }
            else if (typeValue == "boolean") {
                scriptProperty.type = PropertyType::Boolean;
            }
            else if (typeValue == "vec2") {
                scriptProperty.type = PropertyType::Vec2;
            }
            else if (typeValue == "vec3") {
                scriptProperty.type = PropertyType::Vec3;
            }
            else if (typeValue == "vec4") {
                scriptProperty.type = PropertyType::Vec4;
            }
            else {
                continue; // unknown type
            }

            // determine property value
            const auto& value = tbl["value"];
            if (!value.valid())
                continue;
            switch (scriptProperty.type) {
            case PropertyType::String:
                if (!value.is<std::string>())
                    continue;
                scriptProperty.stringValue = value.get<std::string>();
                break;
            case PropertyType::Double:
                if (!value.is<double>())
                    continue;
                scriptProperty.doubleValue = value.get<double>();
                break;
            case PropertyType::Integer:
                if (!value.is<int>())
                    continue;
                scriptProperty.integerValue = value.get<int>();
                break;
            case PropertyType::Boolean:
                if (!value.is<bool>())
                    continue;
                scriptProperty.booleanValue = value.get<bool>();
                break;
            case PropertyType::Vec2:
                if (!value.is<vec2>())
                    continue;
                scriptProperty.vec2Value = value.get<vec2>();
                break;
            case PropertyType::Vec3:
                if (!value.is<vec3>())
                    continue;
                scriptProperty.vec3Value = value.get<vec3>();
                break;
            case PropertyType::Vec4:
                if (!value.is<vec4>())
                    continue;
                scriptProperty.vec3Value = value.get<vec4>();
                break;
            case PropertyType::Undefined:
                break;
            }

            foundProperties[name] = scriptProperty;
        }

        return foundProperties;
    }

    void LuaScriptComponent::GetOrUpdatePropertiesFromScript(bool update) {

        // obtain the new properties from the script
        auto newProperties = GetPropertiesFromScript();
        for (auto& [name, prop] : newProperties) {
            bool oldPropEqualToNewProp = false;

            if (!properties.contains(name))
                continue;

            const auto& existingProperty = properties[name];
            if (!existingProperty.wasChanged && !update)
                continue;

            // Only update if there was a server side change of the script properties
            prop = existingProperty;
        }

        // update the member
        properties = newProperties;
    }

    void LuaScriptComponent::SetPropertyValuesInLuaState() {
        AE_ASSERT(scriptEnvironment.has_value());
        auto& state = scriptEnvironment.value();

        for (const auto& [propertyName, property] : properties) {
            switch (property.type) {
            case PropertyType::String:
                state["ScriptProperties"][propertyName]["value"] = property.stringValue;
                break;
            case PropertyType::Double:
                state["ScriptProperties"][propertyName]["value"] = property.doubleValue;
                break;
            case PropertyType::Integer:
                state["ScriptProperties"][propertyName]["value"] = property.integerValue;
                break;
            case PropertyType::Boolean:
                state["ScriptProperties"][propertyName]["value"] = property.booleanValue;
                break;
            case PropertyType::Vec2:
                state["ScriptProperties"][propertyName]["value"] = property.vec2Value;
                break;
            case PropertyType::Vec3:
                state["ScriptProperties"][propertyName]["value"] = property.vec3Value;
                break;
            case PropertyType::Vec4:
                state["ScriptProperties"][propertyName]["value"] = property.vec4Value;
                break;
            case PropertyType::Undefined:
                break;
            }
        }
    }
}