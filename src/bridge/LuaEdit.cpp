#include "LuaEdit.h"
#include "LuaJob.h"
#include "SceneTools.h"
#include "TerrainTools.h"

namespace Atlas::Bridge {

    using namespace Scene::Components;

    LuaEdit::LuaEdit(const Ref<Scene::Scene>& scene, EditState& state, const EditHooks& hooks)
        : scene(scene), state(state), hooks(hooks) {

    }

    json LuaEdit::Run(const std::string& code, const std::string& name) {

        auto& lua = scene->GetLuaState();
        sol::environment environment(lua, sol::create, lua.globals());

        auto scenePointer = scene.get();
        environment["scene"] = scenePointer;
        environment.set_function("GetThisScene", [scenePointer]() { return scenePointer; });

        auto start = std::chrono::steady_clock::now();
        Scripting::Binder bridge(lua.create_table(), "Bridge");
        AddFunctions(bridge, scene, state, hooks, [this](const std::string& key, json value) { results[key] = std::move(value); },
            [start]() { return SecondsSince(start); });
        environment["Bridge"] = bridge.GetTable();
        if (hooks.extendEnvironment)
            hooks.extendEnvironment(environment);

        auto result = lua.safe_script(code, environment, sol::script_pass_on_error, "=" + name);
        // The functions refer to the hooks of the caller, a script which kept the table only finds it empty later
        bridge.GetTable().clear();
        if (!result.valid()) {
            sol::error error = result;
            throw std::runtime_error("Lua " + name + ": " + error.what());
        }
        if (result.return_count() > 0 && !results.contains("value"))
            results["value"] = LuaJob::ToJson(result.get<sol::object>());

        // Global matrices, bounds and ray casts see the changes in the next command
        scene->UpdateTransforms();

        return results;

    }

    void LuaEdit::AddFunctions(Scripting::Binder& bridge, const Ref<Scene::Scene>& scene, EditState& state, const EditHooks& hooks,
        const std::function<void(const std::string&, json)>& result, const std::function<float()>& time) {

        auto notify = [](const std::function<void(Scene::Entity)>& hook, Scene::Entity entity) {
            if (hook)
                hook(entity);
            };
        bridge.set_function("Find", [scene](sol::this_state s, const std::string& pattern) {
            return ToTable(s, FindEntities(*scene, pattern));
            });
        bridge.set_function("FindMeshes", [scene](sol::this_state s, const std::string& pattern) {
            return ToTable(s, FindMeshInstances(*scene, { pattern }));
            });
        bridge.set_function("Query", [scene](sol::this_state s, sol::object query) {
            auto results = RunQueries(*scene, json::array({ LuaJob::ToJson(query) }));
            return FromJson(s, results.empty() ? json() : results.front());
            });
        bridge.set_function("SampleTerrain", [scene](sol::this_state s, float x, float z) -> sol::object {
            // All cells are loaded on the first call, the heights are the ones of the full detail
            auto sample = SampleTerrain(GetCompleteTerrain(*scene), vec2(x, z));
            if (!sample.valid)
                return sol::lua_nil;
            sol::table table = sol::state_view(s).create_table();
            table["hole"] = sample.hole;
            table["material"] = sample.material;
            if (!sample.hole) {
                table["height"] = sample.height;
                table["slope"] = sample.slopeDegrees;
                table["normal"] = sample.normal;
            }
            return table;
            });
        bridge.set_function("PickRay", [hooks](sol::object camera, float x, float y) {
            if (!hooks.pickRay)
                throw std::runtime_error("PickRay needs a job that renders");
            return hooks.pickRay(LuaJob::ToJson(camera), vec2(x, y));
            });
        bridge.set_function("Result", [result](const std::string& key, sol::object value) { result(key, LuaJob::ToJson(value)); });
        bridge.set_function("Fail", [](const std::string& message) { throw std::runtime_error(message); });
        bridge.set_function("Time", time);
        bridge.set_function("Selection", [hooks](sol::this_state s) {
            return ToTable(s, hooks.selection ? hooks.selection() : std::vector<Scene::Entity>());
            });
        bridge.set_function("Camera", [hooks](sol::this_state s) {
            return FromJson(s, hooks.camera ? hooks.camera() : json());
            });
        bridge.set_function("Touch", [hooks, notify](Scene::Entity entity) { notify(hooks.beforeChange, entity); });
        bridge.set_function("SetPosition", [hooks, notify](Scene::Entity entity, vec3 position) {
            notify(hooks.beforeTransformChange, entity);
            SetWorldPosition(entity, position);
            });
        bridge.set_function("SetMatrix", [hooks, notify](Scene::Entity entity, mat4 matrix) {
            notify(hooks.beforeTransformChange, entity);
            entity.GetComponent<TransformComponent>().Set(matrix);
            });
        bridge.set_function("Create", [scene, hooks, notify, root = std::optional<Scene::Entity>()](
            const std::string& entityName, sol::optional<Scene::Entity> parent) mutable {
            // Looked up once on the first call, it scans all hierarchies
            if (!root)
                root = scene->GetRootEntity();
            auto entity = scene->CreateEntity();
            entity.AddComponent<NameComponent>(entityName);
            entity.AddComponent<TransformComponent>(mat4(1.0f), false);
            // Scenes without a root keep new entities at the top
            auto parentEntity = parent ? *parent : *root;
            if (parentEntity.IsValid()) {
                auto hierarchy = parentEntity.TryGetComponent<HierarchyComponent>();
                if (!hierarchy)
                    hierarchy = &parentEntity.AddComponent<HierarchyComponent>();
                hierarchy->AddChild(entity);
            }
            notify(hooks.created, entity);
            return entity;
            });
        bridge.set_function("Destroy", [scene, hooks, notify](Scene::Entity entity) {
            notify(hooks.beforeDestroy, entity);
            scene->DestroyEntity(entity);
            });
        bridge.set_function("Edit", [scene, &state, hooks](sol::this_state s, sol::object operation) {
            if (hooks.beforeOperation)
                hooks.beforeOperation();
            auto log = ApplyEditOperations(scene, json::array({ LuaJob::ToJson(operation) }), state);
            return FromJson(s, log.empty() ? json() : log.front());
            });

    }

    sol::table LuaEdit::ToTable(sol::state_view lua, const std::vector<Scene::Entity>& entities) {

        sol::table table = lua.create_table(int32_t(entities.size()), 0);
        for (size_t i = 0; i < entities.size(); i++)
            table[i + 1] = entities[i];
        return table;

    }

    sol::object LuaEdit::FromJson(sol::state_view lua, const json& value) {

        if (value.is_null())
            return sol::lua_nil;
        if (value.is_boolean())
            return sol::make_object(lua, value.get<bool>());
        if (value.is_number_integer())
            return sol::make_object(lua, value.get<int64_t>());
        if (value.is_number())
            return sol::make_object(lua, value.get<double>());
        if (value.is_string())
            return sol::make_object(lua, value.get<std::string>());

        sol::table table = lua.create_table();
        if (value.is_array()) {
            for (size_t i = 0; i < value.size(); i++)
                table[i + 1] = FromJson(lua, value[i]);
        }
        else {
            for (const auto& [key, item] : value.items())
                table[key] = FromJson(lua, item);
        }
        return table;

    }

}
