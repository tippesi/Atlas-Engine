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

        if (scriptWasModifiedInLastUpdate) {
            // the script was modified

            // reset the saved references
            updateFunction.reset();
            scriptEnvironment.reset();

            // reload the properties, therefore init the state
            if (!InitScriptEnvironment())
                return;

            // and then get or update the properties from the script
            GetOrUpdatePropertiesFromScript();
        }

        if (scene->physicsWorld->pauseSimulation) {
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

            // create environment based functions
            scriptEnv.set_function("GetThisEntity", [&]() { return this->entity; });

            scriptEnv.set_function("GetThisScene", [&]() { return this->scene; });

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

    std::vector<LuaScriptComponent::ScriptProperty> LuaScriptComponent::GetPropertiesFromScript() {
        AE_ASSERT(scriptEnvironment.has_value());

        const auto& state = scriptEnvironment.value();

        sol::optional<sol::table> scriptProperties = state["ScriptProperties"];
        if (!scriptProperties.has_value()) {
            return {};
        }

        std::vector<ScriptProperty> foundProperties;
        for (const auto& entry : scriptProperties.value()) {
            ScriptProperty scriptProperty = {};

            // determine property name
            const auto& key = entry.first;
            if (!key.valid())
                continue;
            if (!key.is<std::string>())
                continue;
            scriptProperty.name = key.as<std::string>();

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
            }

            foundProperties.push_back(scriptProperty);
        }

        return foundProperties;
    }

    void LuaScriptComponent::GetOrUpdatePropertiesFromScript() {
        // generate a map for the old properties
        std::unordered_map<std::string, int> oldPropertyMap;
        std::set<int> toRemove;
        for (auto i = 0; i < properties.size(); i++) {
            oldPropertyMap[properties[i].name] = i;
            toRemove.insert(i);
        }

        // obtain the new properties from the script
        auto newProperties = GetPropertiesFromScript();
        for (auto& newProp : newProperties) {
            bool oldPropEqualToNewProp = false;
            auto find = oldPropertyMap.find(newProp.name);
            if (find != oldPropertyMap.end()) {
                // a property with this name already exists
                const auto& oldProperty = properties[(*find).second];
                if (newProp.type == oldProperty.type) {
                    // the new and old property also have the same type
                    // therefore it is not necessary to remove the old property
                    toRemove.erase((*find).second);
                    oldPropEqualToNewProp = true;
                }
            }

            if (!oldPropEqualToNewProp) {
                // the new property is not covered by an old one, hence add it
                properties.push_back(newProp);
            }
        }

        // generate the new property set by skipping the ones that are no longer defined
        std::vector<ScriptProperty> mergedProperties;
        for (auto i = 0; i < properties.size(); i++) {
            if (toRemove.contains(i))
                continue;
            mergedProperties.push_back(properties[i]);
        }

        // update the member
        properties = mergedProperties;
    }

    void LuaScriptComponent::SetPropertyValuesInLuaState() {
        AE_ASSERT(scriptEnvironment.has_value());
        auto& state = scriptEnvironment.value();

        for (const auto& property : properties) {
            switch (property.type) {
            case PropertyType::String:
                state["ScriptProperties"][property.name]["value"] = property.stringValue;
                break;
            case PropertyType::Double:
                state["ScriptProperties"][property.name]["value"] = property.doubleValue;
                break;
            case PropertyType::Integer:
                state["ScriptProperties"][property.name]["value"] = property.integerValue;
                break;
            case PropertyType::Boolean:
                state["ScriptProperties"][property.name]["value"] = property.booleanValue;
                break;
            }
        }
    }
}