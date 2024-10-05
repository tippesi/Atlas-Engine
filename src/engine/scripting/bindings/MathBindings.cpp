#include "MathBindings.h"

namespace Atlas::Scripting::Bindings {

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

        GenerateGlmTypeBinding<glm::mat3, float>(ns, "Mat3",
            sol::constructors<glm::mat3(float)>());

        GenerateGlmTypeBinding<glm::mat4, float>(ns, "Mat4",
            sol::constructors<glm::mat4(float)>());

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

    }

}