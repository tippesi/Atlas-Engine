#include "LuaScriptBindings.h"
#include "Log.h"
#include "scene/Entity.h"
#include "scene/Scene.h"
#include "Clock.h"
#include "resource/ResourceManager.h"
#include "input/KeyboardMap.h"

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
        GenerateVolumeBindings(atlasNs);
        GenerateLightingBindings(atlasNs);
        GenerateInputBindings(atlasNs);
        GenerateGraphicBindings(atlasNs);

    }

    void LuaScriptBindings::GenerateSceneBindings(sol::table* ns) {

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
            "ao", &Scene::Scene::ao,
            "reflection", &Scene::Scene::reflection,
            "sss", &Scene::Scene::sss,
            "ssgi", &Scene::Scene::ssgi,
            "sky", &Scene::Scene::sky,
            "postProcessing", &Scene::Scene::postProcessing
            );

        ns->new_usertype<Scene::Wind>("Wind",
            "direction", &Scene::Wind::direction,
            "speed", &Scene::Wind::speed,
            "noiseMap", &Scene::Wind::noiseMap
            );

    }

    void LuaScriptBindings::GenerateEntityBindings(sol::table* ns) {

        auto entityType = ns->new_usertype<Scene::Entity>("Entity",
            "IsValid", &Scene::Entity::IsValid,
            // Add components
            "AddAudioComponent", &Scene::Entity::AddComponent<AudioComponent, ResourceHandle<Audio::AudioData>&, float, bool>,
            "AddAudioVolumeComponent", &Scene::Entity::AddComponent<AudioVolumeComponent, ResourceHandle<Audio::AudioData>&, Volume::AABB&, float>,
            "AddCameraComponent", &Scene::Entity::AddComponent<CameraComponent, float, float, float, float, glm::vec3&, glm::vec2&>,
            "AddHierarchyComponent", &Scene::Entity::AddComponent<HierarchyComponent>,
            "AddLightComponent", &Scene::Entity::AddComponent<LightComponent>,
            "AddMeshComponent", &Scene::Entity::AddComponent<MeshComponent, ResourceHandle<Mesh::Mesh>&>,
            "AddNameComponent", &Scene::Entity::AddComponent<NameComponent, std::string>,
            "AddPlayerComponent", &Scene::Entity::AddComponent<PlayerComponent>,
            "AddRigidBodyComponent", &Scene::Entity::AddComponent<RigidBodyComponent>,
            "AddTextComponent", &Scene::Entity::AddComponent<TextComponent, ResourceHandle<Font>&, std::string>,
            "AddTransformComponent", &Scene::Entity::AddComponent<TransformComponent, glm::mat4&, bool>,

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

        ns->new_usertype<DirectionalLightProperties>("DirectionalLightProperties",
            "direction", &DirectionalLightProperties::direction
        );

        ns->new_usertype<PointLightProperties>("PointLightProperties",
            "position", &PointLightProperties::position,
            "radius", &PointLightProperties::radius,
            "attenuation", &PointLightProperties::attenuation
        );

        ns->new_usertype<TypeProperties>("TypeProperties",
            "directional", &TypeProperties::directional,
            "point", &TypeProperties::point
        );

        // TODO: Extend this
        ns->new_usertype<LightComponent>("LightComponent",
            "color", &LightComponent::color,
            "intensity", &LightComponent::intensity,
            "properties", &LightComponent::properties,
            "transformedProperties", &LightComponent::transformedProperties,
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
            //"Compose", &TransformComponent::Compose,
            "matrix", &TransformComponent::matrix,
            "globalMatrix", &TransformComponent::globalMatrix
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

    }

    void LuaScriptBindings::GenerateUtilityBindings(sol::table* ns) {

        auto log = ns->new_usertype<Log>("Log");
        // Set it manually here to avoid having a function that require the optional argument to log
        log.set_function("Message", [](const std::string& msg) { Log::Message(msg); });
        log.set_function("Warning", [](const std::string& warn) { Log::Warning(warn); });
        log.set_function("Error", [](const std::string& err) { Log::Error(err); });

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

        ns->set_function("LookAt",  [](const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) 
            { return glm::lookAt(eye, center, up); }
            );

        ns->set_function("EulerAngles", [](const glm::quat& quaternion) { return glm::eulerAngles(quaternion); });

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

        auto resizeFrustumOverload = sol::overload(
            [](Volume::Frustum& frustum, const std::vector<vec3>& corners) { frustum.Resize(corners); },
            [](Volume::Frustum& frustum, const mat4& matrix) { frustum.Resize(matrix); }
        );

        ns->new_usertype<Volume::Frustum>("Frustum",
            sol::call_constructor,
            sol::constructors<Volume::Frustum(), Volume::Frustum(const std::vector<vec3>&), Volume::Frustum(glm::mat4)>(),
            "Resize", resizeFrustumOverload,
            "Intersects", &Volume::Frustum::Intersects,
            "IsInside", &Volume::Frustum::IsInside,
            "GetPlanes", &Volume::Frustum::GetPlanes,
            "GetCorners", &Volume::Frustum::GetCorners
            );

    }

    void LuaScriptBindings::GenerateLightingBindings(sol::table* ns) {

        ns->new_usertype<Lighting::VolumetricClouds::Scattering>("VolumetricCloudsScattering",
            "extinctionFactor", &Lighting::VolumetricClouds::Scattering::extinctionFactor,
            "scatteringFactor", &Lighting::VolumetricClouds::Scattering::scatteringFactor,
            "extinctionCoefficients", &Lighting::VolumetricClouds::Scattering::extinctionCoefficients,
            "eccentricityFirstPhase", &Lighting::VolumetricClouds::Scattering::eccentricityFirstPhase,
            "eccentricitySecondPhase", &Lighting::VolumetricClouds::Scattering::eccentricitySecondPhase,
            "phaseAlpha", &Lighting::VolumetricClouds::Scattering::phaseAlpha
            );

        ns->new_usertype<Lighting::VolumetricClouds>("VolumetricClouds",
            "coverageTexture", &Lighting::VolumetricClouds::coverageTexture,
            "shapeTexture", &Lighting::VolumetricClouds::shapeTexture,
            "detailTexture", &Lighting::VolumetricClouds::detailTexture,
            "shadowTexture", &Lighting::VolumetricClouds::shadowTexture,
            "sampleCount", &Lighting::VolumetricClouds::sampleCount,
            "occlusionSampleCount", &Lighting::VolumetricClouds::occlusionSampleCount,
            "shadowSampleFraction", &Lighting::VolumetricClouds::shadowSampleFraction,
            "minHeight", &Lighting::VolumetricClouds::minHeight,
            "maxHeight", &Lighting::VolumetricClouds::maxHeight,
            "distanceLimit", &Lighting::VolumetricClouds::distanceLimit,
            "coverageScale", &Lighting::VolumetricClouds::coverageScale,
            "shapeScale", &Lighting::VolumetricClouds::shapeScale,
            "detailScale", &Lighting::VolumetricClouds::detailScale,
            "coverageSpeed", &Lighting::VolumetricClouds::coverageSpeed,
            "shapeSpeed", &Lighting::VolumetricClouds::shapeSpeed,
            "detailSpeed", &Lighting::VolumetricClouds::detailSpeed,
            "detailStrength", &Lighting::VolumetricClouds::detailStrength,
            "densityMultiplier", &Lighting::VolumetricClouds::densityMultiplier,
            "heightStretch", &Lighting::VolumetricClouds::heightStretch,
            "darkEdgeFocus", &Lighting::VolumetricClouds::darkEdgeFocus,
            "darkEdgeAmbient", &Lighting::VolumetricClouds::darkEdgeAmbient,
            "needsNoiseUpdate", &Lighting::VolumetricClouds::needsNoiseUpdate,
            "enable", &Lighting::VolumetricClouds::enable,
            "castShadow", &Lighting::VolumetricClouds::castShadow,
            "stochasticOcclusionSampling", &Lighting::VolumetricClouds::stochasticOcclusionSampling,
            "scattering", &Lighting::VolumetricClouds::scattering
            );

        ns->new_usertype<Lighting::Atmosphere>("Atmosphere",
            "height", &Lighting::Atmosphere::height
        );

        ns->new_usertype<Lighting::SSS>("SSS",
            "sampleCount", &Lighting::SSS::sampleCount,
            "maxLength", &Lighting::SSS::maxLength,
            "thickness", &Lighting::SSS::thickness,
            "enable", &Lighting::SSS::enable
        );

        ns->new_usertype<Lighting::SSGI>("SSGI",
            "enable", &Lighting::SSGI::enable,
            "enableAo", &Lighting::SSGI::enableAo,
            "radius", &Lighting::SSGI::radius,
            "rayCount", &Lighting::SSGI::rayCount,
            "sampleCount", &Lighting::SSGI::sampleCount,
            "irradianceLimit", &Lighting::SSGI::irradianceLimit,
            "aoStrength", &Lighting::SSGI::aoStrength,
            "rt", &Lighting::SSGI::rt,
            "opacityCheck", &Lighting::SSGI::opacityCheck
        );

        ns->new_usertype<Lighting::Reflection>("Reflection",
            "textureLevel", &Lighting::Reflection::textureLevel,
            "radianceLimit", &Lighting::Reflection::radianceLimit,
            "bias", &Lighting::Reflection::bias,
            "spatialFilterStrength", &Lighting::Reflection::spatialFilterStrength,
            "temporalWeight", &Lighting::Reflection::temporalWeight,
            "historyClipMax", &Lighting::Reflection::historyClipMax,
            "currentClipFactor", &Lighting::Reflection::currentClipFactor,
            "enable", &Lighting::Reflection::enable,
            "rt", &Lighting::Reflection::rt,
            "gi", &Lighting::Reflection::gi,
            "useShadowMap", &Lighting::Reflection::useShadowMap,
            "useNormalMaps", &Lighting::Reflection::useNormalMaps,
            "opacityCheck", &Lighting::Reflection::opacityCheck
        );

        ns->new_usertype<Lighting::EnvironmentProbe>("EnvironmentProbe",
            "resolution", &Lighting::EnvironmentProbe::resolution,
            "viewMatrices", &Lighting::EnvironmentProbe::viewMatrices,
            "projectionMatrix", &Lighting::EnvironmentProbe::projectionMatrix,
            "update", &Lighting::EnvironmentProbe::update,
            "cubemap", &Lighting::EnvironmentProbe::cubemap,
            "depth", &Lighting::EnvironmentProbe::depth,
            "filteredDiffuse", &Lighting::EnvironmentProbe::filteredDiffuse,
            "filteredSpecular", &Lighting::EnvironmentProbe::filteredSpecular
        );

        ns->new_usertype<Lighting::Fog>("Fog",
            "enable", &Lighting::Fog::enable,
            "extinctionFactor", &Lighting::Fog::extinctionFactor,
            "scatteringFactor", &Lighting::Fog::scatteringFactor,
            "extinctionCoefficients", &Lighting::Fog::extinctionCoefficients,
            "density", &Lighting::Fog::density,
            "height", &Lighting::Fog::height,
            "heightFalloff", &Lighting::Fog::heightFalloff,
            "scatteringAnisotropy", &Lighting::Fog::scatteringAnisotropy,
            "ambientFactor", &Lighting::Fog::ambientFactor,
            "rayMarching", &Lighting::Fog::rayMarching,
            "rayMarchStepCount", &Lighting::Fog::rayMarchStepCount,
            "volumetricIntensity", &Lighting::Fog::volumetricIntensity
        );

        ns->new_usertype<Lighting::Sky>("Sky",
            "GetProbe", &Lighting::Sky::GetProbe,
            "planetCenter", &Lighting::Sky::planetCenter,
            "planetRadius", &Lighting::Sky::planetRadius,
            "clouds", &Lighting::Sky::clouds,
            "atmosphere", &Lighting::Sky::atmosphere,
            "probe", &Lighting::Sky::probe
        );

    }

    void LuaScriptBindings::GenerateInputBindings(sol::table* ns) {

        ns->new_enum("Keycode", 
            "KeyUnknown", Keycode::KeyUnknown,
            "KeyEnter", Keycode::KeyEnter,
            "KeyEscape", Keycode::KeyEscape,
            "KeyBackspace", Keycode::KeyBackspace,
            "KeyTab", Keycode::KeyTab,
            "KeySpace", Keycode::KeySpace,
            "KeyExclaim", Keycode::KeyExclaim,
            "KeyDoubleQuote", Keycode::KeyDoubleQuote,
            "KeyHash", Keycode::KeyHash,
            "KeyPercent", Keycode::KeyPercent,
            "KeyDollar", Keycode::KeyDollar,
            "KeyAnd", Keycode::KeyAnd,
            "KeyQuote", Keycode::KeyQuote,
            "KeyLeftParen", Keycode::KeyLeftParen,
            "KeyRightParen", Keycode::KeyRightParen,
            "KeyAsterisk", Keycode::KeyAsterisk,
            "KeyPlus", Keycode::KeyPlus,
            "KeyComma", Keycode::KeyComma,
            "KeyMinus", Keycode::KeyMinus,
            "KeyPeriod", Keycode::KeyPeriod,
            "KeySlash", Keycode::KeySlash,
            "Key0", Keycode::Key0,
            "Key1", Keycode::Key1,
            "Key2", Keycode::Key2,
            "Key3", Keycode::Key3,
            "Key4", Keycode::Key4,
            "Key5", Keycode::Key5,
            "Key6", Keycode::Key6,
            "Key7", Keycode::Key7,
            "Key8", Keycode::Key8,
            "Key9", Keycode::Key9,
            "KeyColon", Keycode::KeyColon,
            "KeySemicolon", Keycode::KeySemicolon,
            "KeyLess", Keycode::KeyLess,
            "KeyEquals", Keycode::KeyEquals,
            "KeyGreater", Keycode::KeyGreater,
            "KeyQuestion", Keycode::KeyQuestion,
            "KeyAt", Keycode::KeyAt,
            "KeyLeftBracket", Keycode::KeyLeftBracket,
            "KeyBackSlash", Keycode::KeyBackSlash,
            "KeyRightBracket", Keycode::KeyRightBracket,
            "KeyCaret", Keycode::KeyCaret,
            "KeyUnderscore", Keycode::KeyUnderscore,
            "KeyBackquote", Keycode::KeyBackquote,
            "KeyA", Keycode::KeyA,
            "KeyB", Keycode::KeyB,
            "KeyC", Keycode::KeyC,
            "KeyD", Keycode::KeyD,
            "KeyE", Keycode::KeyE,
            "KeyF", Keycode::KeyF,
            "KeyG", Keycode::KeyG,
            "KeyH", Keycode::KeyH,
            "KeyI", Keycode::KeyI,
            "KeyJ", Keycode::KeyJ,
            "KeyK", Keycode::KeyK,
            "KeyL", Keycode::KeyL,
            "KeyM", Keycode::KeyM,
            "KeyN", Keycode::KeyN,
            "KeyO", Keycode::KeyO,
            "KeyP", Keycode::KeyP,
            "KeyQ", Keycode::KeyQ,
            "KeyR", Keycode::KeyR,
            "KeyS", Keycode::KeyS,
            "KeyT", Keycode::KeyT,
            "KeyU", Keycode::KeyU,
            "KeyV", Keycode::KeyV,
            "KeyW", Keycode::KeyW,
            "KeyX", Keycode::KeyX,
            "KeyY", Keycode::KeyY,
            "KeyZ", Keycode::KeyZ,
            "KeyCapsLock", Keycode::KeyCapsLock,
            "KeyF1", Keycode::KeyF1,
            "KeyF2", Keycode::KeyF2,
            "KeyF3", Keycode::KeyF3,
            "KeyF4", Keycode::KeyF4,
            "KeyF5", Keycode::KeyF5,
            "KeyF6", Keycode::KeyF6,
            "KeyF7", Keycode::KeyF7,
            "KeyF8", Keycode::KeyF8,
            "KeyF9", Keycode::KeyF9,
            "KeyF10", Keycode::KeyF10,
            "KeyF11", Keycode::KeyF11,
            "KeyF12", Keycode::KeyF12,
            "KeyPrintScreen", Keycode::KeyPrintScreen,
            "KeyScrollLock", Keycode::KeyScrollLock,
            "KeyPause", Keycode::KeyPause,
            "KeyInsert", Keycode::KeyInsert,
            "KeyHome", Keycode::KeyHome,
            "KeyPageUp", Keycode::KeyPageUp,
            "KeyDelete", Keycode::KeyDelete,
            "KeyEnd", Keycode::KeyEnd,
            "KeyPageDown", Keycode::KeyPageDown,
            "KeyRight", Keycode::KeyRight,
            "KeyLeft", Keycode::KeyLeft,
            "KeyDown", Keycode::KeyDown,
            "KeyUp", Keycode::KeyUp
        );

        ns->new_usertype<Input::KeyboardMap>("KeyboardMap",
            "GetKeyState", Input::KeyboardMap::GetKeyState,
            "IsKeyPressed", Input::KeyboardMap::IsKeyPressed
        );

    }

    void LuaScriptBindings::GenerateGraphicBindings(sol::table* ns) {

        ns->new_usertype<Texture::Texture2D>("Texture2D",
            "width", &Texture::Texture2D::width,
            "height", &Texture::Texture2D::height,
            "channels", &Texture::Texture2D::channels
        );

        ns->new_usertype<Texture::Texture2DArray>("Texture2DArrayc",
            "width", &Texture::Texture2DArray::width,
            "height", &Texture::Texture2DArray::height,
            "depth", &Texture::Texture2DArray::depth,
            "channels", &Texture::Texture2DArray::channels
        );

        ns->new_usertype<Texture::Texture3D>("Texture3D",
            "width", &Texture::Texture3D::width,
            "height", &Texture::Texture3D::height,
            "depth", &Texture::Texture3D::depth,
            "channels", &Texture::Texture3D::channels
        );

        ns->new_usertype<Texture::Cubemap>("Cubemap",
            "width", &Texture::Cubemap::width,
            "height", &Texture::Cubemap::height,
            "channels", &Texture::Cubemap::channels
        );

    }

}