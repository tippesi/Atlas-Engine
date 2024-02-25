#include "LuaScriptBindings.h"
#include "Log.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "Clock.h"

namespace Atlas::Scripting
{
    LuaScriptBindings::LuaScriptBindings(Ref<sol::state> luaState, sol::table *atlasNs, sol::table* glmNs)
    {
        this->luaState = luaState;
        this->atlasNs = atlasNs;
        this->glmNs = glmNs;
    }

    void LuaScriptBindings::GenerateBindings()
    {

        auto transformUserType = atlasNs->new_usertype<TransformComponent>("TransformComponent");
        transformUserType["translate"] = &TransformComponent::Translate;
        transformUserType["set"] = &TransformComponent::Set;

        GenerateEntityBindings(atlasNs);
        GenerateComponentBindings(atlasNs);
        GenerateUtilityBindings(atlasNs);
        GenerateMathBindings(glmNs);

    }

    void LuaScriptBindings::GenerateEntityBindings(sol::table* ns) {

        auto entityType = ns->new_usertype<Scene::Entity>(
            "Entity",
            "get_transform_component", &Scene::Entity::TryGetComponent<TransformComponent>
        );

    }

    void LuaScriptBindings::GenerateComponentBindings(sol::table* ns) {



    }

    void LuaScriptBindings::GenerateUtilityBindings(sol::table* ns) {

        ns->new_usertype<Log>("Log",
            "message", &Log::Message,
            "warning", &Log::Warning,
            "error", &Log::Error);

        ns->new_usertype<Clock>("Clock",
            "get", &Clock::Get,
            "get_delta", &Clock::GetDelta,
            "get_average", &Clock::GetAverage);

    }

    void LuaScriptBindings::GenerateMathBindings(sol::table* ns) {

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

        ns->set_function("dot", sol::overload(
            [](const glm::vec2& v0, const glm::vec2& v1) { return glm::dot(v0, v1); },
            [](const glm::vec3& v0, const glm::vec3& v1) { return glm::dot(v0, v1); },
            [](const glm::vec4& v0, const glm::vec4& v1) { return glm::dot(v0, v1); }
        ));

        ns->set_function("translate", sol::overload(
            [](const glm::vec3& vec) { return glm::translate(vec); },
            [](const glm::mat4& mat, const glm::vec3& vec) { return glm::translate(mat, vec); }
        ));

        ns->set_function("scale", sol::overload(
            [](const glm::vec3& vec) { return glm::scale(vec); },
            [](const glm::mat4& mat, const glm::vec3& vec) { return glm::scale(mat, vec); }
        ));

        ns->set_function("rotate", sol::overload(
            [](float angle, const glm::vec3& vec) { return glm::rotate(angle, vec); },
            [](const glm::mat4& mat, float angle, const glm::vec3& vec) { return glm::rotate(mat, angle, vec); }
        ));

    }

}