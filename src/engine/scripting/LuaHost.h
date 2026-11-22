#pragma once

#include "../System.h"
#include "../common/Ref.h"
#include "../scene/Scene.h"
#include "Binder.h"

#include <sol/sol.hpp>

#include <functional>
#include <string>
#include <vector>

namespace Atlas::Scripting {

    // Runs a main script with the engine bindings independent of a scene, e.g. the Main.lua of a project. Its state
    // stays while the scene changes. The script defines Load(), OnSceneLoaded(scene), Update(deltaTime) and
    // OnContact(event), all of them are optional. Applications add their own tables with AddModule before Load()
    class LuaHost {

    public:
        explicit LuaHost(const std::vector<std::string>& args = {});

        // The bindings refer to the host
        LuaHost(const LuaHost&) = delete;

        LuaHost& operator=(const LuaHost&) = delete;

        // Fills the global table name, which is created if it doesn't exist yet. The host itself provides Host
        void AddModule(const std::string& name, const std::function<void(Binder&)>& fill);

        // Runs the script and its Load function
        bool Load(const std::string& path);

        // The scene GetThisScene() returns, calls OnSceneLoaded(scene)
        void SetScene(const Ref<Scene::Scene>& scene);

        // Calls OnContact for the contact events of the last physics update, then Update
        void Update(float deltaTime);

        sol::state& GetState() { return *state; }

        // A script error stops the host, an error in a frame would otherwise repeat every frame
        bool HasFailed() const { return failed; }

        // Set by Host.LoadScene(path), the application loads the scene, calls SetScene and clears it
        std::string requestedScene;
        // Set by Host.Quit()
        bool quitRequested = false;

    private:
        bool Call(sol::protected_function& function, const std::string& name, auto&&... args);

        Ref<sol::state> state;
        Ref<Scene::Scene> scene;
        std::vector<std::string> args;

        std::string path;
        bool failed = false;

        sol::protected_function updateFunction;
        sol::protected_function contactFunction;
        sol::protected_function sceneLoadedFunction;

    };

}
