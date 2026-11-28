#include "LuaHost.h"
#include "LuaScriptBindings.h"

#include "../Log.h"
#include "../loader/AssetLoader.h"

namespace Atlas::Scripting {

    LuaHost::LuaHost(const std::vector<std::string>& args) : args(args) {

        state = CreateRef<sol::state>();
        state->open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

        sol::table atlasNs = (*state)["Atlas"].get_or_create<sol::table>();
        sol::table glmNs = (*state)["Glm"].get_or_create<sol::table>();
        LuaScriptBindings bindings(state, &atlasNs, &glmNs);
        bindings.GenerateBindings();

        Binder globals(state->globals(), "_G");
        globals.set_function("GetThisScene", [this]() { return scene.get(); });

        AddModule("Host", [this](Binder& host) {
            host.set_function("Args", [this](sol::this_state s) {
                sol::state_view lua(s);
                sol::table list = lua.create_table();
                for (size_t i = 0; i < this->args.size(); i++)
                    list[i + 1] = this->args[i];
                return list;
                });
            host.set_function("LoadScene", [this](const std::string& path) { requestedScene = path; });
            host.set_function("Quit", [this]() { quitRequested = true; });
            });

    }

    void LuaHost::AddModule(const std::string& name, const std::function<void(Binder&)>& fill) {

        Binder binder((*state)[name].get_or_create<sol::table>(), name);
        fill(binder);

    }

    bool LuaHost::Load(const std::string& path) {

        this->path = path;

        auto stream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);
        if (!stream.is_open()) {
            Log::Error("Couldn't open " + path);
            failed = true;
            return false;
        }
        std::string code((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

        auto result = state->safe_script(code, sol::script_pass_on_error, "@" + path);
        if (!result.valid()) {
            sol::error error = result;
            Log::Error("Error while loading " + path + ": " + error.what());
            failed = true;
            return false;
        }

        updateFunction = (*state)["Update"];
        contactFunction = (*state)["OnContact"];
        sceneLoadedFunction = (*state)["OnSceneLoaded"];

        sol::protected_function loadFunction = (*state)["Load"];
        return Call(loadFunction, "Load");

    }

    void LuaHost::SetScene(const Ref<Scene::Scene>& scene) {

        this->scene = scene;
        Call(sceneLoadedFunction, "OnSceneLoaded", scene.get());

    }

    void LuaHost::Update(float deltaTime) {

        if (scene && contactFunction.valid()) {
            for (const auto& event : scene->GetContactEvents()) {
                if (!Call(contactFunction, "OnContact", event))
                    return;
            }
        }

        Call(updateFunction, "Update", deltaTime);

    }

    bool LuaHost::Call(sol::protected_function& function, const std::string& name, auto&&... args) {

        if (failed)
            return false;
        if (!function.valid())
            return true;

        auto result = function(std::forward<decltype(args)>(args)...);
        if (!result.valid()) {
            sol::error error = result;
            Log::Error("Error in " + name + " of " + path + ": " + error.what());
            failed = true;
            return false;
        }

        return true;

    }

}
