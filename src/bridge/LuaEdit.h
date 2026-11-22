#pragma once

#include "Common.h"
#include "SceneEditor.h"

#include "scene/Scene.h"
#include "volume/Ray.h"
#include "scripting/Binder.h"

#include <sol/sol.hpp>

#include <functional>

namespace Atlas::Bridge {

    // What a host records while a script edits its scene, e.g. the undo step of the editor. Without a host none is set
    struct EditHooks {
        std::function<void(Scene::Entity)> beforeChange;
        std::function<void(Scene::Entity)> beforeTransformChange;
        std::function<void(Scene::Entity)> created;
        std::function<void(Scene::Entity)> beforeDestroy;
        // Before the first edit operation, which can't say what it touches
        std::function<void()> beforeOperation;
        // The selection and camera of the editor
        std::function<std::vector<Scene::Entity>()> selection;
        std::function<json()> camera;
        // The ray through a pixel of a shot camera, in jobs that render
        std::function<Volume::Ray(const json& camera, vec2 pixel)> pickRay;
        // Adds tables of the host, e.g. Editor
        std::function<void(sol::environment& environment)> extendEnvironment;
    };

    // Lua that changes and queries a scene, in edits (the lua operation, editor commands) and in the Lua of jobs: the code
    // runs over the globals of the scene's script state with the engine bindings, scene (GetThisScene()) and the table
    // Bridge. Changes made through Bridge are recorded by the hooks, scripts which change components directly call
    // Bridge.Touch(entity) first. McpTools::GetLuaHelp describes the functions, the bindings tool lists them with types
    class LuaEdit {

    public:
        LuaEdit(const Ref<Scene::Scene>& scene, EditState& state, const EditHooks& hooks = {});

        // Returns the results, Lua errors throw with the line
        json Run(const std::string& code, const std::string& name);

        static sol::object FromJson(sol::state_view lua, const json& value);

        // Fills the table Bridge, the hooks and the edit state have to outlive it
        static void AddFunctions(Scripting::Binder& bridge, const Ref<Scene::Scene>& scene, EditState& state, const EditHooks& hooks,
            const std::function<void(const std::string&, json)>& result, const std::function<float()>& time);

    private:
        static sol::table ToTable(sol::state_view lua, const std::vector<Scene::Entity>& entities);

        Ref<Scene::Scene> scene;
        EditState& state;
        EditHooks hooks;

        json results = json::object();

    };

}
