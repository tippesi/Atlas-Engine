#include "LuaJob.h"
#include "Report.h"

namespace Atlas::Capture {

    // Expressions are evaluated as "return <code>", statements as they are
    static bool LoadCode(lua_State* lua, const std::string& code, const std::string& name, bool expression) {

        auto chunkName = "=" + name;
        if (expression) {
            auto returned = "return " + code;
            if (luaL_loadbuffer(lua, returned.data(), returned.size(), chunkName.c_str()) == LUA_OK)
                return true;
            lua_pop(lua, 1);
        }

        return luaL_loadbuffer(lua, code.data(), code.size(), chunkName.c_str()) == LUA_OK;

    }

    void LuaJob::Start(sol::state& luaState, Scene::Scene* scene, Report& report, const std::function<float()>& time) {

        state = &luaState;
        environment = sol::environment(luaState, sol::create, luaState.globals());

        auto& env = *environment;
        env["scene"] = scene;
        env.set_function("GetThisScene", [scene]() { return scene; });

        sol::table capture = luaState.create_table();
        capture.set_function("Result", [&report](const std::string& key, sol::object value) {

            report.results[key] = ToJson(value);
            report.Print("result " + key + " = " + report.results[key].dump());

        });
        capture.set_function("Time", time);
        capture.set_function("Fail", [](const std::string& message) { throw std::runtime_error(message); });
        env["Capture"] = capture;

    }

    void LuaJob::Stop() {

        environment.reset();
        state = nullptr;

    }

    json LuaJob::Run(const std::string& code, const std::string& name) {

        if (!environment)
            throw std::runtime_error("Lua " + name + ": no script state");

        auto result = state->safe_script(code, *environment, sol::script_pass_on_error, name);
        if (!result.valid()) {
            sol::error error = result;
            throw std::runtime_error("Lua " + name + ": " + error.what());
        }

        return result.return_count() > 0 ? ToJson(result.get<sol::object>()) : json();

    }

    json LuaJob::Evaluate(const std::string& code, const std::string& name) {

        if (!environment)
            throw std::runtime_error("Lua " + name + ": no script state");

        auto lua = state->lua_state();
        if (!LoadCode(lua, code, name, true)) {
            std::string message = lua_tostring(lua, -1);
            lua_pop(lua, 1);
            throw std::runtime_error("Lua " + name + ": " + message);
        }

        sol::protected_function function(lua, -1);
        lua_pop(lua, 1);
        sol::set_environment(*environment, function);

        auto result = function();
        if (!result.valid()) {
            sol::error error = result;
            throw std::runtime_error("Lua " + name + ": " + error.what());
        }

        return result.return_count() > 0 ? ToJson(result.get<sol::object>()) : json();

    }

    json LuaJob::Expect(const json& expectation, bool& passed) {

        auto code = expectation.at("lua").get<std::string>();
        auto actual = Evaluate(code, "expectation");

        json result = { { "expect", expectation }, { "actual", actual } };
        if (!expectation.contains("op")) {
            passed = IsTrue(actual);
            return result;
        }

        auto op = expectation["op"].get<std::string>();
        const auto& expected = expectation.at("value");
        if (actual.is_number() && expected.is_number()) {
            auto a = actual.get<double>(), b = expected.get<double>();
            auto tolerance = expectation.value("tolerance", 0.01);
            if (op == "==")
                passed = std::abs(a - b) <= tolerance;
            else if (op == "!=")
                passed = std::abs(a - b) > tolerance;
            else if (op == ">")
                passed = a > b;
            else if (op == ">=")
                passed = a >= b - tolerance;
            else if (op == "<")
                passed = a < b;
            else if (op == "<=")
                passed = a <= b + tolerance;
            else
                throw std::runtime_error("Unknown op '" + op + "' (==, !=, >, >=, <, <=)");
        }
        else if (op == "==" || op == "!=") {
            passed = (actual == expected) == (op == "==");
        }
        else {
            throw std::runtime_error("The lua expectation " + code + " returned " + actual.dump() + ", '" + op + "' needs numbers");
        }

        return result;

    }

    bool LuaJob::IsTrue(const json& value) {

        return !value.is_null() && value != false;

    }

    std::string LuaJob::Compile(const std::string& code, const std::string& name, bool expression) {

        auto lua = luaL_newstate();
        std::string error;
        if (!LoadCode(lua, code, name, expression))
            error = lua_tostring(lua, -1);
        lua_close(lua);
        return error;

    }

    json LuaJob::ToJson(const sol::object& object, int32_t depth) {

        switch (object.get_type()) {
        case sol::type::lua_nil:
        case sol::type::none:
            return json();
        case sol::type::boolean:
            return object.as<bool>();
        case sol::type::number:
            if (object.is<int64_t>() && double(object.as<int64_t>()) == object.as<double>())
                return object.as<int64_t>();
            return object.as<double>();
        case sol::type::string:
            return object.as<std::string>();
        case sol::type::table: {
            if (depth > 8)
                return "<table>";
            auto table = object.as<sol::table>();
            auto length = table.size();
            if (length > 0) {
                json array = json::array();
                for (size_t i = 1; i <= length; i++)
                    array.push_back(ToJson(table[i], depth + 1));
                return array;
            }
            json map = json::object();
            for (const auto& [key, value] : table)
                map[key.is<std::string>() ? key.as<std::string>() : ToJson(key).dump()] = ToJson(value, depth + 1);
            return map;
        }
        case sol::type::userdata:
            if (object.is<vec2>()) {
                auto v = object.as<vec2>();
                return json::array({ v.x, v.y });
            }
            if (object.is<vec3>())
                return FromVec3(object.as<vec3>());
            if (object.is<vec4>()) {
                auto v = object.as<vec4>();
                return json::array({ v.x, v.y, v.z, v.w });
            }
            break;
        default:
            break;
        }

        // Anything else as Lua would print it
        auto lua = object.lua_state();
        object.push(lua);
        size_t length = 0;
        auto text = luaL_tolstring(lua, -1, &length);
        std::string result(text, length);
        lua_pop(lua, 2);
        return result;

    }

}
