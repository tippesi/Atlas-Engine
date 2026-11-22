#pragma once

#include "Common.h"

#include "LuaEdit.h"
#include "SceneEditor.h"
#include "scene/Scene.h"

#include <sol/sol.hpp>

#include <functional>
#include <optional>

namespace Atlas::Bridge {

    class Report;

    // The Lua of a job: lua events, conditions and expectations. Snippets run in one environment over the globals of a
    // script state (the scene's or the one of a game host), so they see what the game scripts put there and share their
    // variables. The environment has scene (GetThisScene()) and the table Bridge of LuaEdit.h: Result(key, value) stores
    // a value in report.json, Time() is the time of the job, Fail(message) fails it
    class LuaJob {

    public:
        // Bridge.Edit uses the edit state of the job, without one its own
        void Start(sol::state& state, Scene::Scene* scene, Report& report, const std::function<float()>& time,
            const EditHooks& hooks = {}, EditState* jobEditState = nullptr);

        // Empties the table Bridge, its functions refer to this object and the edit state
        void Stop();

        bool IsStarted() const { return environment.has_value(); }

        // Runs a chunk, Lua errors throw with the name of the snippet
        json Run(const std::string& code, const std::string& name);

        // Evaluates an expression, or a chunk that returns a value
        json Evaluate(const std::string& code, const std::string& name);

        // A lua expectation or condition: { "lua": "...", "op": ">=", "value": 3 }, without op the value has to be true
        json Expect(const json& expectation, bool& passed);

        // Only nil and false are false, like in Lua
        static bool IsTrue(const json& value);

        // Compiles the code like Evaluate would without running it, returns the error or an empty string
        static std::string Compile(const std::string& code, const std::string& name, bool expression);

        static json ToJson(const sol::object& object, int32_t depth = 0);

    private:
        sol::state* state = nullptr;
        std::optional<sol::environment> environment;
        sol::table bridgeTable;
        // Edit operations of Bridge.Edit
        EditState editState;

    };

}
