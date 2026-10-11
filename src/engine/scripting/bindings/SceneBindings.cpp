#include "SceneBindings.h"
#include "MemberBindings.h"

#include "scene/Scene.h"
#include "scene/components/LuaScriptComponent.h"

namespace Atlas::Scripting::Bindings {

    void GenerateSceneBindings(sol::table* ns) {

        ns->new_usertype<Scene::Wind>("Wind",
            "direction", &Scene::Wind::direction,
            "speed", &Scene::Wind::speed,
            "noiseMap", &Scene::Wind::noiseMap
            );

        ns->new_usertype<Scene::Scene>("Scene",
            "CreateEntity", &Scene::Scene::CreateEntity,
            "DestroyEntity", &Scene::Scene::DestroyEntity,
            "DuplicateEntity", &Scene::Scene::DuplicateEntity,
            "GetEntityByName", &Scene::Scene::GetEntityByName,
            "GetParentEntity", &Scene::Scene::GetParentEntity,
            "GetEntityCount", &Scene::Scene::GetEntityCount,
            "GetMainCamera", &Scene::Scene::GetMainCamera,
            "HasMainCamera", &Scene::Scene::HasMainCamera,
            "GetMeshes", &Scene::Scene::GetMeshes,
            "wind", &Scene::Scene::wind,
            "sky", &Scene::Scene::sky,
            "fog", &Scene::Scene::fog,
            "irradianceVolume", &Scene::Scene::irradianceVolume,
            "reflection", &Scene::Scene::reflection,
            "sss", &Scene::Scene::sss,
            "ssgi", &Scene::Scene::ssgi,
            "postProcessing", &Scene::Scene::postProcessing
            );

    }

    void GenerateEntityBindings(sol::table* ns) {

        auto entityType = ns->new_usertype<Scene::Entity>("Entity",
            "IsValid", &Scene::Entity::IsValid,
            "GetID", &Scene::Entity::GetID,
            "GetVersion", &Scene::Entity::GetVersion,
            // Add components
            "AddAudioComponent", &Scene::Entity::AddComponent<AudioComponent, ResourceHandle<Audio::AudioData>&, float, bool>,
            "AddAudioVolumeComponent", &Scene::Entity::AddComponent<AudioVolumeComponent, ResourceHandle<Audio::AudioData>&, Volume::AABB&, float>,
            "AddCameraComponent", &Scene::Entity::AddComponent<CameraComponent, float, float, float, float, glm::vec3&, glm::vec2&>,
            "AddHierarchyComponent", &Scene::Entity::AddComponent<HierarchyComponent>,
            "AddLightComponent", &Scene::Entity::AddComponent<LightComponent>,
            "AddMeshComponent", &Scene::Entity::AddComponent<MeshComponent, ResourceHandle<Mesh::Mesh>&>,
            "AddNameComponent", &Scene::Entity::AddComponent<NameComponent, std::string>,
            "AddPlayerComponent", &Scene::Entity::AddComponent<PlayerComponent>,
            "AddRigidBodyComponent", &Scene::Entity::AddComponent<RigidBodyComponent, Physics::BodyCreationSettings&>,
            "AddTextComponent", &Scene::Entity::AddComponent<TextComponent, ResourceHandle<Font>&, std::string>,
            "AddTransformComponent", &Scene::Entity::AddComponent<TransformComponent, glm::mat4&, bool>,
            "AddLuaScriptComponent", &Scene::Entity::AddComponent<LuaScriptComponent, ResourceHandle<Script>&>,
            "AddSplineComponent", &Scene::Entity::AddComponent<SplineComponent>,

            // Remove components
            "RemoveAudioComponent", &Scene::Entity::RemoveComponent<AudioComponent>,
            "RemoveAudioVolumeComponent", &Scene::Entity::RemoveComponent<AudioVolumeComponent>,
            "RemoveCameraComponent", &Scene::Entity::RemoveComponent<CameraComponent>,
            "RemoveHierarchyComponent", &Scene::Entity::RemoveComponent<HierarchyComponent>,
            "RemoveLightComponent", &Scene::Entity::RemoveComponent<LightComponent>,
            "RemoveMeshComponent", &Scene::Entity::RemoveComponent<MeshComponent>,
            "RemoveNameComponent", &Scene::Entity::RemoveComponent<NameComponent>,
            "RemovePlayerComponent", &Scene::Entity::RemoveComponent<PlayerComponent>,
            "RemoveRigidBodyComponent", &Scene::Entity::RemoveComponent<RigidBodyComponent>,
            "RemoveTextComponent", &Scene::Entity::RemoveComponent<TextComponent>,
            "RemoveTransformComponent", &Scene::Entity::RemoveComponent<TransformComponent>,
            "RemoveLuaScriptComponent", &Scene::Entity::RemoveComponent<LuaScriptComponent>,
            "RemoveSplineComponent", &Scene::Entity::RemoveComponent<SplineComponent>,

            // Get components
            "GetAudioComponent", &Scene::Entity::TryGetComponent<AudioComponent>,
            "GetAudioVolumeComponent", &Scene::Entity::TryGetComponent<AudioVolumeComponent>,
            "GetCameraComponent", &Scene::Entity::TryGetComponent<CameraComponent>,
            "GetHierarchyComponent", &Scene::Entity::TryGetComponent<HierarchyComponent>,
            "GetLightComponent", &Scene::Entity::TryGetComponent<LightComponent>,
            "GetMeshComponent", &Scene::Entity::TryGetComponent<MeshComponent>,
            "GetNameComponent", &Scene::Entity::TryGetComponent<NameComponent>,
            "GetPlayerComponent", &Scene::Entity::TryGetComponent<PlayerComponent>,
            "GetRigidBodyComponent", &Scene::Entity::TryGetComponent<RigidBodyComponent>,
            "GetTextComponent", &Scene::Entity::TryGetComponent<TextComponent>,
            "GetTransformComponent", &Scene::Entity::TryGetComponent<TransformComponent>,
            "GetLuaScriptComponent", &Scene::Entity::TryGetComponent<LuaScriptComponent>,
            "GetSplineComponent", &Scene::Entity::TryGetComponent<SplineComponent>
        );

    }

    void GenerateComponentBindings(sol::table* ns) {

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

        ns->new_usertype<DirectionalLightProperties>("DirectionalLightProperties",
            "direction", &DirectionalLightProperties::direction
        );

        ns->new_usertype<PointLightProperties>("PointLightProperties",
            "position", &PointLightProperties::position,
            "radius", &PointLightProperties::radius
        );

        ns->new_usertype<SpotLightProperties>("SpotLightProperties",
            "position", &SpotLightProperties::position,
            "direction", &SpotLightProperties::direction,
            "radius", &SpotLightProperties::radius,
            "outerConeAngle", &SpotLightProperties::outerConeAngle,
            "innerConeAngle", &SpotLightProperties::innerConeAngle
        );

        ns->new_usertype<TypeProperties>("TypeProperties",
            "directional", &TypeProperties::directional,
            "point", &TypeProperties::point,
            "spot", &TypeProperties::spot
        );

        ns->new_enum<LightType>("LightType", {
           { "DirectionalLight", LightType::DirectionalLight },
           { "PointLight", LightType::PointLight },
           { "SpotLight", LightType::SpotLight }
           });

        ns->new_enum<LightMobility>("LightMobility", {
          { "StationaryLight", LightMobility::StationaryLight },
          { "MovableLight", LightMobility::MovableLight }
          });

        ns->new_usertype<LightComponent>("LightComponent",
            "AddPointShadow", &LightComponent::AddPointShadow,
            "AddSpotShadow", &LightComponent::AddSpotShadow,
            "IsVisible", &LightComponent::IsVisible,
            "type", &LightComponent::type,
            "mobility", &LightComponent::mobility,
            "color", &LightComponent::color,
            "intensity", &LightComponent::intensity,
            "volumetricIntensity", &LightComponent::volumetricIntensity,
            "properties", &LightComponent::properties,
            "transformedProperties", &LightComponent::transformedProperties,
            "shadow", &LightComponent::shadow,
            "isMain", &LightComponent::isMain,
            "volumetric", &LightComponent::volumetric
        );

        ns->new_usertype<MeshComponent>("MeshComponent",
            "mesh", &MeshComponent::mesh,
            "visible", &MeshComponent::visible,
            "dontCull", &MeshComponent::dontCull,
            "aabb", &MeshComponent::aabb
        );

        ns->new_usertype<NameComponent>("NameComponent",
            "name", &NameComponent::name
        );

        ns->new_usertype<TransformComponent>("TransformComponent",
            "Translate", &TransformComponent::Translate,
            "Set", &TransformComponent::Set,
            "Decompose", &TransformComponent::Decompose,
            "DecomposeGlobal", &TransformComponent::DecomposeGlobal,
            "ReconstructLocalMatrix", &TransformComponent::ReconstructLocalMatrix,
            "matrix", &TransformComponent::matrix,
            "globalMatrix", &TransformComponent::globalMatrix,
            "isStatic", &TransformComponent::isStatic
        );

        ns->new_usertype<RigidBodyComponent>("RigidBodyComponent",
            "IsValid", &RigidBodyComponent::IsValid,
            "SetMatrix", &RigidBodyComponent::SetMatrix,
            "MoveKinematic", &RigidBodyComponent::MoveKinematic,
            "GetMatrix", &RigidBodyComponent::GetMatrix,
            "SetMotionQuality", &RigidBodyComponent::SetMotionQuality,
            "GetMotionQuality", &RigidBodyComponent::GetMotionQuality,
            "SetLinearVelocity", &RigidBodyComponent::SetLinearVelocity,
            "GetLinearVelocity", &RigidBodyComponent::GetLinearVelocity,
            "SetAngularVelocity", &RigidBodyComponent::SetAngularVelocity,
            "GetAngularVelocity", &RigidBodyComponent::GetAngularVelocity,
            "SetRestitution", &RigidBodyComponent::SetRestitution,
            "GetRestitution", &RigidBodyComponent::GetRestitution,
            "SetFriction", &RigidBodyComponent::SetFriction,
            "GetFriction", &RigidBodyComponent::GetFriction,
            "GetBodyCreationSettings", &RigidBodyComponent::GetBodyCreationSettings,
            "layer", &RigidBodyComponent::layer
        );

        ns->new_usertype<PlayerComponent>("PlayerComponent",
            "SetPosition", &PlayerComponent::SetPosition,
            "GetPosition", &PlayerComponent::GetPosition,
            "SetRotation", &PlayerComponent::SetRotation,
            "GetRotation", &PlayerComponent::GetRotation,
            "GetMatrix", &PlayerComponent::GetMatrix,
            "SetLinearVelocity", &PlayerComponent::SetLinearVelocity,
            "GetLinearVelocity", &PlayerComponent::GetLinearVelocity,
            "GetGroundVelocity", &PlayerComponent::GetGroundVelocity,
            "IsOnGround", &PlayerComponent::IsOnGround,
            "stickToGroundDist", &PlayerComponent::stickToGroundDist,
            "walkStairsStepUpDist", &PlayerComponent::walkStairsStepUpDist
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

        ns->new_enum<LuaScriptComponent::PropertyType>("ScriptPropertyType", {
            { "Undefined", LuaScriptComponent::PropertyType::Undefined },
            { "String", LuaScriptComponent::PropertyType::String },
            { "Double", LuaScriptComponent::PropertyType::Double },
            { "Int", LuaScriptComponent::PropertyType::Integer },
            { "Bool", LuaScriptComponent::PropertyType::Boolean },
            });

        ns->new_usertype<LuaScriptComponent>("LuaScriptComponent",
            "ChangeResource", &LuaScriptComponent::ChangeResource,
            "HasProperty", &LuaScriptComponent::HasProperty,
            "GetPropertyType", &LuaScriptComponent::GetPropertyType,
            "SetPropertyString", &LuaScriptComponent::SetPropertyValue<std::string>,
            "SetPropertyDouble", &LuaScriptComponent::SetPropertyValue<double>,
            "SetPropertyInt", &LuaScriptComponent::SetPropertyValue<int32_t>,
            "SetPropertyBool", &LuaScriptComponent::SetPropertyValue<bool>,
            "GetPropertyString", &LuaScriptComponent::GetPropertyValue<std::string>,
            "GetPropertyDouble", &LuaScriptComponent::GetPropertyValue<double>,
            "GetPropertyInt", &LuaScriptComponent::GetPropertyValue<int32_t>,
            "GetPropertyBool", &LuaScriptComponent::GetPropertyValue<bool>,
            "permanentExecution", &LuaScriptComponent::permanentExecution,
            "script", &LuaScriptComponent::script
        );

        ns->new_enum<SplineType>("SplineType", {
            { "Linear", SplineType::Linear },
            { "CatmullRom", SplineType::CatmullRom },
            { "Hermite", SplineType::Hermite }
            });

        ns->new_enum<SplineBakeMode>("SplineBakeMode", {
            { "NoBake", SplineBakeMode::NoBake },
            { "EqualSpacingBake", SplineBakeMode::EqualSpacingBake },
            { "DistanceSpacingBake", SplineBakeMode::DistanceSpacingBake }
            });

        ns->new_usertype<SplineControlPoint>("SplineControlPoint",
            sol::call_constructor,
            sol::constructors<SplineControlPoint(), SplineControlPoint(glm::mat4, float)>(),
            "transform", DependentMember<&SplineControlPoint::transform>(),
            "tangent", DependentMember<&SplineControlPoint::tangent>(),
            "time", &SplineControlPoint::time
            );

        ns->new_usertype<SplinePoint>("SplinePoint",
            "position", DependentMember<&SplinePoint::position>(),
            "rotation", DependentMember<&SplinePoint::rotation>(),
            "scale", DependentMember<&SplinePoint::scale>(),
            "tangent", DependentMember<&SplinePoint::tangent>(),
            "time", &SplinePoint::time
        );

        ns->new_usertype<SplineComponent>("SplineComponent",
            "AddControlPoint", &SplineComponent::AddControlPoint,
            "ClearControlPoints", &SplineComponent::ClearControlPoints,
            "GetControlPoint", &SplineComponent::GetControlPoint,
            "GetControlPointCount", &SplineComponent::GetControlPointCount,
            "Bake", &SplineComponent::Bake,
            "GetInterpolated", &SplineComponent::GetInterpolated,
            "type", &SplineComponent::type,
            "bakeMode", &SplineComponent::bakeMode
        );

    }

}
