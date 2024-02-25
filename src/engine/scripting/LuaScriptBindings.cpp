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
        GenerateSceneBindings(atlasNs);
        GenerateEntityBindings(atlasNs);
        GenerateComponentBindings(atlasNs);
        GenerateUtilityBindings(atlasNs);
        GenerateMathBindings(glmNs);
    }

    void LuaScriptBindings::GenerateSceneBindings(sol::table* ns) {

        ns->new_usertype<Scene::Scene>("Scene",
            "CreateEntity", &Scene::Scene::CreateEntity,
            "DestroyEntity", &Scene::Scene::DestroyEntity,
            "DuplicateEntity", &Scene::Scene::DuplicateEntity,
            "GetEntityByName", &Scene::Scene::GetEntityByName,
            "GetParentEntity", &Scene::Scene::GetParentEntity);

    }

    void LuaScriptBindings::GenerateEntityBindings(sol::table* ns) {

        auto entityType = ns->new_usertype<Scene::Entity>("Entity",
            "IsValid", &Scene::Entity::IsValid,
            // Add components
            "AddAudioComponent", &Scene::Entity::AddComponent<AudioComponent>,
            "AddAudioVolumeComponent", &Scene::Entity::AddComponent<AudioVolumeComponent>,
            "AddCameraComponent", &Scene::Entity::AddComponent<CameraComponent>,
            "AddHierarchyComponent", &Scene::Entity::AddComponent<HierarchyComponent>,
            "AddLightComponentComponent", &Scene::Entity::AddComponent<LightComponent>,
            "AddMeshComponent", &Scene::Entity::AddComponent<MeshComponent>,
            "AddNameComponent", &Scene::Entity::AddComponent<NameComponent>,
            "AddPlayerComponent", &Scene::Entity::AddComponent<PlayerComponent>,
            "AddRigidBodyComponent", &Scene::Entity::AddComponent<RigidBodyComponent>,
            "AddTextComponent", &Scene::Entity::AddComponent<TextComponent>,
            "AddTransformComponent", &Scene::Entity::AddComponent<TransformComponent>,

            // Remove components
            "RemoveAudioComponent", &Scene::Entity::RemoveComponent<AudioComponent>,
            "RemoveAudioVolumeComponent", &Scene::Entity::RemoveComponent<AudioVolumeComponent>,
            "RemoveCameraComponent", &Scene::Entity::RemoveComponent<CameraComponent>,
            "RemoveHierarchyComponent", &Scene::Entity::RemoveComponent<HierarchyComponent>,
            "RemoveLightComponentComponent", &Scene::Entity::RemoveComponent<LightComponent>,
            "RemoveMeshComponent", &Scene::Entity::RemoveComponent<MeshComponent>,
            "RemoveNameComponent", &Scene::Entity::RemoveComponent<NameComponent>,
            "RemovePlayerComponent", &Scene::Entity::RemoveComponent<PlayerComponent>,
            "RemoveRigidBodyComponent", &Scene::Entity::RemoveComponent<RigidBodyComponent>,
            "RemoveTextComponent", &Scene::Entity::RemoveComponent<TextComponent>,
            "RemoveTransformComponent", &Scene::Entity::RemoveComponent<TransformComponent>,

            // Get components
            "GetAudioComponent", &Scene::Entity::TryGetComponent<AudioComponent>,
            "GetAudioVolumeComponent", &Scene::Entity::TryGetComponent<AudioVolumeComponent>,
            "GetCameraComponent", &Scene::Entity::TryGetComponent<CameraComponent>,
            "GetHierarchyComponent", &Scene::Entity::TryGetComponent<HierarchyComponent>,
            "GetLightComponentComponent", &Scene::Entity::TryGetComponent<LightComponent>,
            "GetMeshComponent", &Scene::Entity::TryGetComponent<MeshComponent>,
            "GetNameComponent", &Scene::Entity::TryGetComponent<NameComponent>,
            "GetPlayerComponent", &Scene::Entity::TryGetComponent<PlayerComponent>,
            "GetRigidBodyComponent", &Scene::Entity::TryGetComponent<RigidBodyComponent>,
            "GetTextComponent", &Scene::Entity::TryGetComponent<TextComponent>,
            "GetTransformComponent", &Scene::Entity::TryGetComponent<TransformComponent>
        );

    }

    void LuaScriptBindings::GenerateComponentBindings(sol::table* ns) {

        ns->new_usertype<TransformComponent>("TransformComponent",
            "Translate", &TransformComponent::Translate,
            "Set", &TransformComponent::Set,
            "Decompose", &TransformComponent::Decompose,
            //"Compose", &TransformComponent::Compose,
            "matrix", &TransformComponent::matrix,
            "globalMatrix", &TransformComponent::globalMatrix
            );

        ns->new_usertype<NameComponent>("NameComponent",
            "name", &NameComponent::name
        );

        ns->new_usertype<RigidBodyComponent>("RigidBodyComponent",
            "IsValid", &RigidBodyComponent::IsValid,
            "SetMatrix", &RigidBodyComponent::SetMatrix,
            "GetMatrix", &RigidBodyComponent::GetMatrix,
            "SetLinearVelocity", &RigidBodyComponent::SetLinearVelocity,
            "GetLinearVelocity", &RigidBodyComponent::GetLinearVelocity,
            "SetRestitution", &RigidBodyComponent::SetRestitution,
            "GetRestitution", &RigidBodyComponent::GetRestitution,
            "SetFriction", &RigidBodyComponent::SetFriction,
            "GetFriction", &RigidBodyComponent::GetFriction
        );

        ns->new_usertype<TextComponent>("TextComponent",
            "text", &TextComponent::text,
            "position", &TextComponent::position,
            "rotation", &TextComponent::rotation,
            "halfSize", &TextComponent::halfSize,
            "textColor", &TextComponent::textColor,
            "outlineColor", &TextComponent::outlineColor,
            "outlineFactor", &TextComponent::outlineFactor,
            "textScale", &TextComponent::textScale
        );

    }

    void LuaScriptBindings::GenerateUtilityBindings(sol::table* ns) {

        ns->new_usertype<Log>("Log",
            "Message", &Log::Message,
            "Warning", &Log::Warning,
            "Error", &Log::Error);

        ns->new_usertype<Clock>("Clock",
            "Get", &Clock::Get,
            "GetDelta", &Clock::GetDelta,
            "GetAverage", &Clock::GetAverage);

        ns->new_usertype<Common::MatrixDecomposition>("MatrixDecomposition",
            "Compose", &Common::MatrixDecomposition::Compose,
            "Decompose", &Common::MatrixDecomposition::Decompose,
            "translation", &Common::MatrixDecomposition::translation,
            "rotation", &Common::MatrixDecomposition::rotation,
            "scale", &Common::MatrixDecomposition::scale,
            "perspective", &Common::MatrixDecomposition::perspective,
            "skew", &Common::MatrixDecomposition::skew,
            "quaternion", &Common::MatrixDecomposition::quaternion);

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

        /*
        GenerateGlmTypeBinding<glm::quat, float>(ns, "Quat",
            sol::constructors<glm::quat(float), glm::quat(float, float, float, float)>(),
            "x", &glm::quat::x,
            "y", &glm::quat::y,
            "z", &glm::quat::z,
            "w", &glm::quat::w);
        */

        GenerateGlmTypeBinding<glm::mat3, float>(ns, "Mat3",
            sol::constructors<glm::mat3(float)>());

        GenerateGlmTypeBinding<glm::mat4, float>(ns, "Mat4",
            sol::constructors<glm::mat4(float)>());

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

    }

}