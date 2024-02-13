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

        // check if script was modified the frame before
        auto resource = script.GetResource();
        if (scriptWasModifiedInLastUpdate && resource->WasModified())
        {
            resource->UpdateModifiedTime();
        }
        scriptWasModifiedInLastUpdate = false;
        if (resource->WasModified())
        {
            // Do reload here, adjust modified time beforehand to be conservative
            script->Reload();
            scriptWasModifiedInLastUpdate = true;

            // reset the script
            scriptUpdate.reset();
            luaState = nullptr;

            // reload the properties, therefore init the state
            InitLuaState();
            // and then get or update the properties from the script
            GetOrUpdatePropertiesFromScript();
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

        // process the script
        AE_ASSERT(luaState != nullptr);
        SetPropertyValuesInLuaState();

        // call the update function
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

            ns.set_function("log", [](std::string msg)
                            { Log::Message(msg); });

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

    std::vector<LuaScriptComponent::ScriptProperty> LuaScriptComponent::GetPropertiesFromScript()
    {
        AE_ASSERT(luaState != nullptr);

        auto &state = *luaState;

        auto scriptProperties = state["script_properties"];
        if (!scriptProperties.valid())
        {
            return {};
        }
        if (!scriptProperties.is<sol::table>())
        {
            return {};
        }

        std::vector<ScriptProperty> foundProperties;
        for (auto &entry : scriptProperties.get<sol::table>())
        {
            ScriptProperty scriptProperty;

            // determine property name
            const auto &key = entry.first;
            if (!key.valid())
                continue;
            if (!key.is<std::string>())
                continue;
            scriptProperty.name = key.as<std::string>();

            // determine property type and value
            const auto &propertyTable = entry.second;
            if (!propertyTable.valid())
                continue;
            if (!propertyTable.is<sol::table>())
                continue;
            const auto &tbl = propertyTable.as<sol::table>();
            if (!tbl.valid())
                continue;

            // determine property type
            const auto &typeField = tbl["type"];
            if (!typeField.valid())
                continue;
            if (!typeField.is<std::string>())
                continue;
            auto type = typeField.get<std::string>();
            if (type == "string")
            {
                scriptProperty.type = PropertyType::String;
            }
            else if (type == "double")
            {
                scriptProperty.type = PropertyType::Double;
            }
            else if (type == "integer")
            {
                scriptProperty.type = PropertyType::Integer;
            }
            else if (type == "boolean")
            {
                scriptProperty.type = PropertyType::Boolean;
            }
            else
            {
                continue; // unknown type
            }

            // determine property value
            const auto &value = tbl["value"];
            if (!value.valid())
                continue;
            switch (scriptProperty.type)
            {
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

    void LuaScriptComponent::GetOrUpdatePropertiesFromScript()
    {
        AE_ASSERT(luaState != nullptr);

        // generate a map for the old properties
        std::unordered_map<std::string, int> oldPropertyMap;
        std::set<int> toRemove;
        for (auto i = 0; i < properties.size(); i++)
        {
            oldPropertyMap[properties[i].name] = i;
            toRemove.insert(i);
        }

        // obtain the new properties from the script
        auto newProperties = GetPropertiesFromScript();
        for (auto &newProp : newProperties)
        {
            bool oldPropEqualToNewProp = false;
            auto find = oldPropertyMap.find(newProp.name);
            if (find != oldPropertyMap.end())
            {
                // a property with this name already exists
                const auto &oldProperty = properties[(*find).second];
                if (newProp.type == oldProperty.type)
                {
                    // the new and old property also have the same type
                    // therefore it is not necessary to remove the old property
                    toRemove.erase((*find).second);
                    oldPropEqualToNewProp = true;
                }
            }

            if (!oldPropEqualToNewProp)
            {
                // the new property is not covered by an old one, hence add it
                properties.push_back(newProp);
            }
        }

        // generate the new property set by skipping the ones that are no longer defined
        std::vector<ScriptProperty> mergedProperties;
        for (auto i = 0; i < properties.size(); i++)
        {
            if (toRemove.contains(i))
                continue;
            mergedProperties.push_back(properties[i]);
        }

        // update the member
        properties = mergedProperties;
    }

    void LuaScriptComponent::SetPropertyValuesInLuaState()
    {
        AE_ASSERT(luaState != nullptr);
        auto &state = *luaState;

        for (const auto &property : properties)
        {
            switch (property.type)
            {
            case PropertyType::String:
                state["script_properties"][property.name]["value"] = property.stringValue;
                break;
            case PropertyType::Double:
                state["script_properties"][property.name]["value"] = property.doubleValue;
                break;
            case PropertyType::Integer:
                state["script_properties"][property.name]["value"] = property.integerValue;
                break;
            case PropertyType::Boolean:
                state["script_properties"][property.name]["value"] = property.booleanValue;
                break;
            }
        }
    }
}