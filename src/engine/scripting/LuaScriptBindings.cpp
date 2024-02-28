#include "LuaScriptBindings.h"
#include "Log.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "Clock.h"
#include "resource/ResourceManager.h"

namespace Atlas::Scripting {

    LuaScriptBindings::LuaScriptBindings(Ref<sol::state> luaState, sol::table* atlasNs, sol::table* glmNs) {

        this->luaState = luaState;
        this->atlasNs = atlasNs;
        this->glmNs = glmNs;

    }

    void LuaScriptBindings::GenerateBindings() {

        GenerateSceneBindings(atlasNs);
        GenerateEntityBindings(atlasNs);
        GenerateComponentBindings(atlasNs);
        GenerateUtilityBindings(atlasNs);
        GenerateMaterialBindings(atlasNs);
        GenerateAudioBindings(atlasNs);
        GenerateMeshBindings(atlasNs);
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
            "AddAudioComponent", &Scene::Entity::AddComponent<AudioComponent, ResourceHandle<Audio::AudioData>&, float, bool>,
            "AddAudioVolumeComponent", &Scene::Entity::AddComponent<AudioVolumeComponent, ResourceHandle<Audio::AudioData>&, Volume::AABB&, float>,
            "AddCameraComponent", &Scene::Entity::AddComponent<CameraComponent, float, float, float, float, glm::vec3&, glm::vec2&>,
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

        ns->new_usertype<AudioComponent>("AudioComponent",
            "falloffFactor", &AudioComponent::falloffFactor,
            "falloffPower", &AudioComponent::falloffPower,
            "cutoff", &AudioComponent::cutoff,
            "volume", &AudioComponent::volume,
            "stream", &AudioComponent::stream
        );

        ns->new_usertype<AudioVolumeComponent>("AudioVolumeComponent",
            "falloffFactor", &AudioVolumeComponent::falloffFactor,
            "falloffPower", &AudioVolumeComponent::falloffPower,
            "cutoff", &AudioVolumeComponent::cutoff,
            "volume", &AudioVolumeComponent::volume,
            "aabb", &AudioVolumeComponent::aabb,
            "stream", &AudioVolumeComponent::stream
        );

        ns->new_usertype<CameraComponent>("CameraComponent",
            "GetJitter", &CameraComponent::GetJitter,
            "GetLastJitter", &CameraComponent::GetLastJitter,
            "GetLastJitteredMatrix", &CameraComponent::GetLastJitteredMatrix,
            "GetLocation", &CameraComponent::GetLocation,
            "GetLastLocation", &CameraComponent::GetLastLocation,
            "GetFrustumCorners", &CameraComponent::GetFrustumCorners,
            "UpdateFrustum", &CameraComponent::UpdateFrustum,
            "location", &CameraComponent::location,
            "rotation", &CameraComponent::rotation,
            "exposure", &CameraComponent::exposure,
            "fieldOfView", &CameraComponent::fieldOfView,
            "aspectRatio", &CameraComponent::aspectRatio,
            "nearPlane", &CameraComponent::nearPlane,
            "farPlane", &CameraComponent::farPlane,
            "thirdPerson", &CameraComponent::thirdPerson,
            "thirdPersonDistance", &CameraComponent::thirdPersonDistance,
            "direction", &CameraComponent::direction,
            "up", &CameraComponent::up,
            "right", &CameraComponent::right,
            "viewMatrix", &CameraComponent::viewMatrix,
            "projectionMatrix", &CameraComponent::projectionMatrix,
            "invViewMatrix", &CameraComponent::invViewMatrix,
            "invProjectionMatrix", &CameraComponent::invProjectionMatrix,
            "unjitterdProjection", &CameraComponent::unjitterdProjection,
            "invUnjitteredProjection", &CameraComponent::invUnjitteredProjection,
            "parentTransform", &CameraComponent::parentTransform,
            "frustum", &CameraComponent::frustum,
            "isMain", &CameraComponent::isMain,
            "useEntityTranslation", &CameraComponent::useEntityTranslation,
            "useEntityRotation", &CameraComponent::useEntityRotation
        );

        ns->new_usertype<HierarchyComponent>("HierarchyComponent",
            "AddChild", &HierarchyComponent::AddChild,
            "RemoveChild", &HierarchyComponent::RemoveChild,
            "GetChildren", &HierarchyComponent::GetChildren,
            "globalMatrix", &HierarchyComponent::globalMatrix,
            "root", &HierarchyComponent::root
        );

        ns->new_usertype<TransformComponent>("TransformComponent",
            "Translate", &TransformComponent::Translate,
            "Set", &TransformComponent::Set,
            "Decompose", &TransformComponent::Decompose,
            "DecomposeGlobal", &TransformComponent::DecomposeGlobal,
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
            "SetAngularVelocity", &RigidBodyComponent::SetAngularVelocity,
            "GetAngularVelocity", &RigidBodyComponent::GetAngularVelocity,
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
            sol::call_constructor,
            sol::constructors<Common::MatrixDecomposition(glm::mat4)>(),
            "Compose", &Common::MatrixDecomposition::Compose,
            "Decompose", &Common::MatrixDecomposition::Decompose,
            "translation", &Common::MatrixDecomposition::translation,
            "rotation", &Common::MatrixDecomposition::rotation,
            "scale", &Common::MatrixDecomposition::scale,
            "perspective", &Common::MatrixDecomposition::perspective,
            "skew", &Common::MatrixDecomposition::skew,
            "quaternion", &Common::MatrixDecomposition::quaternion);

    }

    void LuaScriptBindings::GenerateMaterialBindings(sol::table* ns) {

         ns->new_usertype<Material>("Material",
            "HasBaseColorMap", &Material::HasBaseColorMap,
            "HasOpacityMap", &Material::HasOpacityMap,
            "HasNormalMap", &Material::HasNormalMap,
            "HasRoughnessMap", &Material::HasRoughnessMap,
            "HasMetalnessMap", &Material::HasMetalnessMap,
            "HasAoMap", &Material::HasAoMap,
            "HasDisplacementMap", &Material::HasDisplacementMap,
            "name", &Material::name,
            "baseColor", &Material::baseColor,
            "transmissiveColor", &Material::transmissiveColor,
            "emissiveColor", &Material::emissiveColor,
            "emissiveIntensity", &Material::emissiveIntensity,
            "opacity", &Material::opacity,
            "roughness", &Material::roughness,
            "metalness", &Material::metalness,
            "ao", &Material::ao,
            "reflectance", &Material::reflectance,
            "normalScale", &Material::normalScale,
            "displacementScale", &Material::displacementScale,
            "tiling", &Material::tiling,
            "twoSided", &Material::twoSided,
            "vertexColors", &Material::vertexColors
            );

    }

    void LuaScriptBindings::GenerateAudioBindings(sol::table* ns) {

        GenerateResourceBinding<Audio::AudioData>(ns, "AudioResourceHandle");

        ns->new_usertype<Audio::AudioData>("AudioData",
            "GetChannelCount", &Audio::AudioData::GetChannelCount,
            "GetFrequency", &Audio::AudioData::GetFrequency,
            "GetSampleSize", &Audio::AudioData::GetSampleSize,
            "filename", &Audio::AudioData::filename
            );

        ns->new_usertype<Audio::AudioStream>("AudioStream",
            "GetDuration", &Audio::AudioStream::GetDuration,
            "SetTime", &Audio::AudioStream::SetTime,
            "GetTime", &Audio::AudioStream::GetTime,
            "SetVolume", &Audio::AudioStream::SetVolume,
            "GetVolume", &Audio::AudioStream::GetVolume,
            "SetPitch", &Audio::AudioStream::SetPitch,
            "GetPitch", &Audio::AudioStream::GetPitch,
            "Pause", &Audio::AudioStream::Pause,
            "Resume", &Audio::AudioStream::Resume,
            "IsPaused", &Audio::AudioStream::IsPaused,
            "IsValid", &Audio::AudioStream::IsValid,
            "loop", &Audio::AudioStream::loop
            );

    }

    void LuaScriptBindings::GenerateMeshBindings(sol::table* ns) {

        GenerateResourceBinding<Mesh::Mesh>(ns, "MeshResourceHandle");

        ns->new_usertype<Mesh::MeshData>("MeshData",
            "filename", &Mesh::MeshData::filename,
            "materials", &Mesh::MeshData::materials,
            "primitiveType", &Mesh::MeshData::primitiveType,
            "aabb", &Mesh::MeshData::aabb,
            "transform", &Mesh::MeshData::transform,
            "radius", &Mesh::MeshData::radius
            );
        
        ns->new_usertype<Mesh::Mesh>("Mesh",
            "name", &Mesh::Mesh::name,
            "data", &Mesh::Mesh::data,
            "mobility", &Mesh::Mesh::mobility,
            "usage", &Mesh::Mesh::usage,
            "cullBackFaces", &Mesh::Mesh::cullBackFaces,
            "depthTest", &Mesh::Mesh::depthTest,
            "castShadow", &Mesh::Mesh::castShadow,
            "vegetation", &Mesh::Mesh::vegetation,
            "windNoiseTextureLod", &Mesh::Mesh::windNoiseTextureLod,
            "windBendScale", &Mesh::Mesh::windBendScale,
            "windWiggleScale", &Mesh::Mesh::windWiggleScale,
            "allowedShadowCascades", &Mesh::Mesh::allowedShadowCascades,
            "impostorDistance", &Mesh::Mesh::impostorDistance,
            "impostorShadowDistance", &Mesh::Mesh::impostorShadowDistance,
            "invertUVs", &Mesh::Mesh::invertUVs
            );

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

    void LuaScriptBindings::GenerateVolumeBindings(sol::table* ns) {

        auto isInsideAABBOverload = sol::overload(
            [](Volume::AABB& aabb0, const Volume::AABB& aabb1) { return aabb0.IsInside(aabb1); },
            [](Volume::AABB& aabb, const vec3& vec) { return aabb.IsInside(vec); }
        );

        auto growAABBOverload = sol::overload(
            [](Volume::AABB& aabb0, const Volume::AABB& aabb1) { return aabb0.Grow(aabb1); },
            [](Volume::AABB& aabb, const vec3& vec) { return aabb.Grow(vec); }
        );

        ns->new_usertype<Volume::AABB>("AABB",
            sol::call_constructor,
            sol::constructors<Volume::AABB(), Volume::AABB(glm::vec3, glm::vec3)>(),
            "Intersects", &Volume::AABB::Intersects,
            "IsInside", isInsideAABBOverload,
            "Transform", &Volume::AABB::Transform,
            "Translate", &Volume::AABB::Translate,
            "Scale", &Volume::AABB::Scale,
            "Grow", growAABBOverload,
            "Intersect", &Volume::AABB::Intersect,
            "GetSurfaceArea", &Volume::AABB::GetSurfaceArea,
            "GetSize", &Volume::AABB::GetSize,
            "GetDistance", &Volume::AABB::GetDistance,
            "GetCorners", &Volume::AABB::GetCorners,
            "min", &Volume::AABB::min,
            "max", &Volume::AABB::max
            );

        // TODO
        ns->new_usertype<Volume::Frustum>("Frustum"
            );

    }

}