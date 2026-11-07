#include "MathBindings.h"

#include <glm/gtx/polar_coordinates.hpp>

namespace Atlas::Scripting::Bindings {

    // Replace sol's metamethods of T for speed, other keys go to sol's metamethod (upvalue 1). Vectors and quaternions
    // take x, y, z and w, matrices the 1-based index of an element, column by column like glm
    template<class T>
    static float* Components(lua_State* state) {

        return reinterpret_cast<float*>(*static_cast<T**>(sol::detail::align_usertype_pointer(lua_touserdata(state, 1))));

    }

    template<class T, bool indexed>
    static int ComponentIndex(lua_State* state) {

        constexpr int count = int(sizeof(T) / sizeof(float));
        if constexpr (indexed) {
            if (!lua_isinteger(state, 2))
                return -1;
            auto index = int(lua_tointeger(state, 2)) - 1;
            return index >= 0 && index < count ? index : -1;
        }
        else {
            size_t length = 0;
            auto key = lua_type(state, 2) == LUA_TSTRING ? lua_tolstring(state, 2, &length) : nullptr;
            if (length != 1)
                return -1;
            auto index = key[0] == 'w' ? 3 : key[0] - 'x';
            return index >= 0 && index < count ? index : -1;
        }

    }

    static int ForwardComponentAccess(lua_State* state, int arguments) {

        auto original = lua_upvalueindex(1);
        if (lua_type(state, original) == LUA_TFUNCTION) {
            lua_pushvalue(state, original);
            lua_insert(state, 1);
            lua_call(state, arguments, arguments == 2 ? 1 : 0);
            return arguments == 2 ? 1 : 0;
        }

        if (arguments == 2) {
            lua_pushvalue(state, 2);
            lua_gettable(state, original);
            return 1;
        }
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_settable(state, original);
        return 0;

    }

    template<class T, bool indexed>
    static int IndexComponent(lua_State* state) {

        auto index = ComponentIndex<T, indexed>(state);
        if (index < 0)
            return ForwardComponentAccess(state, 2);

        lua_pushnumber(state, Components<T>(state)[index]);
        return 1;

    }

    template<class T, bool indexed>
    static int NewIndexComponent(lua_State* state) {

        auto index = ComponentIndex<T, indexed>(state);
        if (index < 0)
            return ForwardComponentAccess(state, 3);

        Components<T>(state)[index] = float(luaL_checknumber(state, 3));
        return 0;

    }

    // Values, references and constant references of T, unique pointers to glm types aren't used
    template<class T, bool indexed = false>
    static void AddComponentAccess(lua_State* state) {

        for (const auto& metatable : { sol::usertype_traits<T>::metatable(), sol::usertype_traits<T*>::metatable(),
            sol::usertype_traits<const T*>::metatable(), sol::usertype_traits<const T>::metatable() }) {
            if (luaL_getmetatable(state, metatable.c_str()) != LUA_TTABLE) {
                lua_pop(state, 1);
                continue;
            }
            lua_getfield(state, -1, "__index");
            lua_pushcclosure(state, &IndexComponent<T, indexed>, 1);
            lua_setfield(state, -2, "__index");
            lua_getfield(state, -1, "__newindex");
            lua_pushcclosure(state, &NewIndexComponent<T, indexed>, 1);
            lua_setfield(state, -2, "__newindex");
            lua_pop(state, 1);
        }

    }

    // The arithmetic of each type written into an existing object, the operators create a new one every time
    template<class... T>
    static void GenerateArithmeticToBindings(sol::table* ns) {

        ns->set_function("AddTo", sol::overload(
            [](T& out, const T& v0, const T& v1) { out = v0 + v1; }...,
            [](T& out, const T& v0, float value) { out = v0 + value; }...,
            [](T& out, float value, const T& v0) { out = v0 + value; }...
        ));

        ns->set_function("SubTo", sol::overload(
            [](T& out, const T& v0, const T& v1) { out = v0 - v1; }...,
            [](T& out, const T& v0, float value) { out = v0 - value; }...,
            [](T& out, float value, const T& v0) { out = value - v0; }...
        ));

        ns->set_function("MulTo", sol::overload(
            [](T& out, const T& v0, const T& v1) { out = v0 * v1; }...,
            [](T& out, const T& v0, float value) { out = v0 * value; }...,
            [](T& out, float value, const T& v0) { out = v0 * value; }...,
            [](glm::vec3& out, const glm::mat3& mat, const glm::vec3& vec) { out = mat * vec; },
            [](glm::vec4& out, const glm::mat4& mat, const glm::vec4& vec) { out = mat * vec; },
            [](glm::quat& out, const glm::quat& v0, const glm::quat& v1) { out = v0 * v1; },
            [](glm::quat& out, const glm::quat& v0, float value) { out = v0 * value; },
            [](glm::vec3& out, const glm::quat& quat, const glm::vec3& vec) { out = quat * vec; }
        ));

        ns->set_function("DivTo", sol::overload(
            [](T& out, const T& v0, const T& v1) { out = v0 / v1; }...,
            [](T& out, const T& v0, float value) { out = v0 / value; }...,
            [](T& out, float value, const T& v0) { out = value / v0; }...
        ));

        ns->set_function("CopyTo", sol::overload(
            [](T& out, const T& value) { out = value; }...,
            [](glm::quat& out, const glm::quat& value) { out = value; }
        ));

    }

    void GenerateMathBindings(sol::table* ns) {

        GenerateGlmTypeBinding<glm::vec2, float>(ns, "Vec2",
            sol::constructors<glm::vec2(float), glm::vec2(float, float)>(),
            "x", &glm::vec2::x,
            "y", &glm::vec2::y);

        GenerateGlmTypeBinding<glm::vec3, float>(ns, "Vec3",
            sol::constructors<glm::vec3(float), glm::vec3(float, float, float)>(),
            "x", &glm::vec3::x,
            "y", &glm::vec3::y,
            "z", &glm::vec3::z);

        GenerateGlmTypeBinding<glm::vec4, float>(ns, "Vec4",
            sol::constructors<glm::vec4(float), glm::vec4(float, float, float, float)>(),
            "x", &glm::vec4::x,
            "y", &glm::vec4::y,
            "z", &glm::vec4::z,
            "w", &glm::vec4::w);

        AddComponentAccess<glm::vec2>(ns->lua_state());
        AddComponentAccess<glm::vec3>(ns->lua_state());
        AddComponentAccess<glm::vec4>(ns->lua_state());

        GenerateGlmTypeBinding<glm::mat3, float>(ns, "Mat3",
            sol::constructors<glm::mat3(float)>());

        GenerateGlmTypeBinding<glm::mat4, float>(ns, "Mat4",
            sol::constructors<glm::mat4(float), glm::mat4(const glm::mat4&)>());

        AddComponentAccess<glm::mat3, true>(ns->lua_state());
        AddComponentAccess<glm::mat4, true>(ns->lua_state());

         auto quat_multiplication_overloads = sol::overload(
            [](const glm::quat& v0, const glm::quat& v1) { return v0 * v1; },
            [](const glm::quat& v0, float value) { return v0 * value; },
            [](const float& value, const glm::quat& v0) { return v0 * value; }
        );

        ns->new_usertype<glm::quat>("Quat",
            sol::call_constructor,
            sol::constructors<glm::quat(glm::vec3), glm::quat(float, float, float, float)>(),
            sol::meta_function::multiplication, quat_multiplication_overloads,
            "x", &glm::quat::x,
            "y", &glm::quat::y,
            "z", &glm::quat::z,
            "w", &glm::quat::w
            );

        AddComponentAccess<glm::quat>(ns->lua_state());

        ns->set_function("Dot", sol::overload(
            [](const glm::vec2& v0, const glm::vec2& v1) { return glm::dot(v0, v1); },
            [](const glm::vec3& v0, const glm::vec3& v1) { return glm::dot(v0, v1); },
            [](const glm::vec4& v0, const glm::vec4& v1) { return glm::dot(v0, v1); }
        ));

        ns->set_function("Translate", sol::overload(
            [](const glm::vec3& vec) { return glm::translate(vec); },
            [](const glm::mat4& mat, const glm::vec3& vec) { return glm::translate(mat, vec); }
        ));

        ns->set_function("Scale", sol::overload(
            [](const glm::vec3& vec) { return glm::scale(vec); },
            [](const glm::mat4& mat, const glm::vec3& vec) { return glm::scale(mat, vec); }
        ));

        ns->set_function("Rotate", sol::overload(
            [](float angle, const glm::vec3& vec) { return glm::rotate(angle, vec); },
            [](const glm::mat4& mat, float angle, const glm::vec3& vec) { return glm::rotate(mat, angle, vec); }
        ));

        ns->set_function("Mix", sol::overload(
            [](const float float0, const float float1, const float factor) { return glm::mix(float0, float1, factor); },
            [](const glm::vec2& vec0, const glm::vec2& vec1, const float factor) { return glm::mix(vec0, vec1, factor); },
            [](const glm::vec3& vec0, const glm::vec3& vec1, const float factor) { return glm::mix(vec0, vec1, factor); },
            [](const glm::quat& quat0, const glm::quat& quat1, const float factor) { return glm::mix(quat0, quat1, factor); }
        ));

        ns->set_function("Clamp", sol::overload(
            [](const float float0, const float float1, const float float2) { return glm::clamp(float0, float1, float2); },
            [](const int32_t int0, const int32_t int1, const int32_t int2) { return glm::clamp(int0, int1, int2); },
            [](const glm::vec2& vec0, const glm::vec2& vec1, const glm::vec2& vec2) { return glm::clamp(vec0, vec1, vec2); },
            [](const glm::vec3& vec0, const glm::vec3& vec1, const glm::vec3& vec2) { return glm::clamp(vec0, vec1, vec2); },
            [](const glm::vec4& vec0, const glm::vec4& vec1, const glm::vec4& vec2) { return glm::clamp(vec0, vec1, vec2); }
        ));

        ns->set_function("Max", sol::overload(
            [](const float float0, const float float1) { return glm::max(float0, float1); },
            [](const glm::vec2& vec0, const glm::vec2& vec1) { return glm::max(vec0, vec1); },
            [](const glm::vec3& vec0, const glm::vec3& vec1) { return glm::max(vec0, vec1); },
            [](const glm::vec4& vec0, const glm::vec4& vec1) { return glm::max(vec0, vec1); }
        ));

        ns->set_function("Min", sol::overload(
            [](const float float0, const float float1) { return glm::min(float0, float1); },
            [](const glm::vec2& vec0, const glm::vec2& vec1) { return glm::min(vec0, vec1); },
            [](const glm::vec3& vec0, const glm::vec3& vec1) { return glm::min(vec0, vec1); },
            [](const glm::vec4& vec0, const glm::vec4& vec1) { return glm::min(vec0, vec1); }
        ));

        ns->set_function("LookAt",  [](const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) 
            { return glm::lookAt(eye, center, up); }
            );

        ns->set_function("EulerAngles", [](const glm::quat& quaternion) { return glm::eulerAngles(quaternion); });

        ns->set_function("Euclidean", [](const glm::vec2& vec) { return glm::euclidean(vec); });
        ns->set_function("Polar", [](const glm::vec3& vec) { return vec2(glm::polar(vec)); });

        // Variants that write into an existing object instead of returning a new one
        // sol tries the overloads in order, the scripts use matrices and 3D vectors most
        GenerateArithmeticToBindings<glm::mat4, glm::vec3, glm::vec2, glm::vec4, glm::mat3>(ns);

        ns->set_function("TranslateTo", sol::overload(
            [](glm::mat4& out, const glm::vec3& vec) { out = glm::translate(vec); },
            [](glm::mat4& out, const glm::mat4& mat, const glm::vec3& vec) { out = glm::translate(mat, vec); }
        ));

        ns->set_function("ScaleTo", sol::overload(
            [](glm::mat4& out, const glm::vec3& vec) { out = glm::scale(vec); },
            [](glm::mat4& out, const glm::mat4& mat, const glm::vec3& vec) { out = glm::scale(mat, vec); }
        ));

        ns->set_function("RotateTo", sol::overload(
            [](glm::mat4& out, float angle, const glm::vec3& vec) { out = glm::rotate(angle, vec); },
            [](glm::mat4& out, const glm::mat4& mat, float angle, const glm::vec3& vec) { out = glm::rotate(mat, angle, vec); }
        ));

        ns->set_function("MixTo", sol::overload(
            [](glm::vec2& out, const glm::vec2& vec0, const glm::vec2& vec1, const float factor) { out = glm::mix(vec0, vec1, factor); },
            [](glm::vec3& out, const glm::vec3& vec0, const glm::vec3& vec1, const float factor) { out = glm::mix(vec0, vec1, factor); },
            [](glm::vec4& out, const glm::vec4& vec0, const glm::vec4& vec1, const float factor) { out = glm::mix(vec0, vec1, factor); },
            [](glm::quat& out, const glm::quat& quat0, const glm::quat& quat1, const float factor) { out = glm::mix(quat0, quat1, factor); }
        ));

        ns->set_function("ClampTo", sol::overload(
            [](glm::vec2& out, const glm::vec2& vec0, const glm::vec2& vec1, const glm::vec2& vec2) { out = glm::clamp(vec0, vec1, vec2); },
            [](glm::vec3& out, const glm::vec3& vec0, const glm::vec3& vec1, const glm::vec3& vec2) { out = glm::clamp(vec0, vec1, vec2); },
            [](glm::vec4& out, const glm::vec4& vec0, const glm::vec4& vec1, const glm::vec4& vec2) { out = glm::clamp(vec0, vec1, vec2); }
        ));

        ns->set_function("MaxTo", sol::overload(
            [](glm::vec2& out, const glm::vec2& vec0, const glm::vec2& vec1) { out = glm::max(vec0, vec1); },
            [](glm::vec3& out, const glm::vec3& vec0, const glm::vec3& vec1) { out = glm::max(vec0, vec1); },
            [](glm::vec4& out, const glm::vec4& vec0, const glm::vec4& vec1) { out = glm::max(vec0, vec1); }
        ));

        ns->set_function("MinTo", sol::overload(
            [](glm::vec2& out, const glm::vec2& vec0, const glm::vec2& vec1) { out = glm::min(vec0, vec1); },
            [](glm::vec3& out, const glm::vec3& vec0, const glm::vec3& vec1) { out = glm::min(vec0, vec1); },
            [](glm::vec4& out, const glm::vec4& vec0, const glm::vec4& vec1) { out = glm::min(vec0, vec1); }
        ));

        ns->set_function("LookAtTo", [](glm::mat4& out, const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
            out = glm::lookAt(eye, center, up);
            });

        ns->set_function("EulerAnglesTo", [](glm::vec3& out, const glm::quat& quaternion) { out = glm::eulerAngles(quaternion); });

        ns->set_function("EuclideanTo", [](glm::vec3& out, const glm::vec2& vec) { out = glm::euclidean(vec); });
        ns->set_function("PolarTo", [](glm::vec2& out, const glm::vec3& vec) { out = vec2(glm::polar(vec)); });

    }

}
