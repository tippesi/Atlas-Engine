#include "SceneBindings.h"
#include "MemberBindings.h"

#include "scene/Scene.h"
#include "scene/components/LuaScriptComponent.h"
#include "volume/Ray.h"

namespace Atlas::Scripting::Bindings {

    struct RayHit {
        bool valid = false;
        float hitDistance = 0.0f;
        vec3 position = vec3(0.0f);
        vec3 normal = vec3(0.0f);
        Scene::Entity entity;
    };

    static bool IsIgnored(Scene::Scene& scene, Scene::Entity entity, Scene::Entity ignored) {

        if (!ignored.IsValid())
            return false;

        // The ignored entity and all of its children are skipped
        while (entity.IsValid()) {
            if (entity.GetID() == ignored.GetID())
                return true;
            entity = scene.GetParentEntity(entity);
        }

        return false;

    }

    static RayHit CastRay(Scene::Scene& scene, const Volume::Ray& inputRay,
        Scene::SceneQueryComponents components, Scene::Entity ignored) {

        RayHit hit;

        if (glm::length(inputRay.direction) == 0.0f)
            return hit;

        // Normalized such that distances are in world units
        auto ray = Volume::Ray(inputRay.origin, glm::normalize(inputRay.direction), inputRay.tMin, inputRay.tMax);
        auto result = scene.CastRay(ray, components, [&](const Scene::Entity& entity) {
            return IsIgnored(scene, entity, ignored);
            });

        if (!result.valid)
            return hit;

        hit.valid = true;
        hit.hitDistance = result.hitDistance;
        hit.position = ray.Get(result.hitDistance);
        hit.normal = result.normal;
        hit.entity = result.data;

        return hit;

    }

    // Components move in memory when their pool grows or another component of the type is removed.
    // Scripts keep components across frames, so they get a reference that looks the component up on
    // every access and forwards it to the component's bindings.
    template<class Comp>
    struct ComponentRef {
        Scene::Entity entity;
    };

    // Raises a Lua error, which jumps over the caller: no objects with destructors may be alive at the call
    template<class Comp>
    static Comp* ResolveComponent(const ComponentRef<Comp>& ref, lua_State* state) {

        auto component = ref.entity.IsValid() ? ref.entity.template TryGetComponent<Comp>() : nullptr;
        if (!component)
            luaL_error(state, "The entity doesn't have the component anymore");
        return component;

    }

    template<class Comp>
    static sol::object GetComponentRef(Scene::Entity& entity, sol::this_state state) {

        if (!entity.IsValid() || !entity.HasComponent<Comp>())
            return sol::lua_nil;
        return sol::make_object(state, ComponentRef<Comp>{ entity });

    }

    template<class Comp, class... Args>
    static ComponentRef<Comp> AddComponentRef(Scene::Entity& entity, Args... args) {

        entity.AddComponent<Comp>(std::forward<Args>(args)...);
        return { entity };

    }

    template<class Comp>
    static void GenerateComponentRefBindings(sol::table* ns, const std::string& name) {

        sol::state_view lua(ns->lua_state());

        // Methods are called with the reference as self and need the component instead. Forwarding happens
        // in Lua, so errors of the method propagate as they are.
        auto resolve = [](const ComponentRef<Comp>& ref, sol::this_state state) {
            return ResolveComponent(ref, state);
        };
        sol::protected_function makeForwardFactory = lua.load(R"(
            local resolve = ...
            return function(method)
                return function(self, ...)
                    return method(resolve(self), ...)
                end
            end
        )");
        sol::protected_function makeForward = makeForwardFactory(sol::as_function(resolve));

        // Forwarding functions don't depend on the component, they are created once per method
        auto methodsName = name + "RefMethods";
        auto makeForwardName = name + "RefMakeForward";
        lua.registry()[methodsName] = lua.create_table();
        lua.registry()[makeForwardName] = makeForward;

        ns->new_usertype<ComponentRef<Comp>>(name + "Ref", sol::no_constructor,
            sol::meta_function::index, [methodsName, makeForwardName](const ComponentRef<Comp>& ref, sol::stack_object key,
                sol::this_state state) -> sol::object {
                {
                    sol::table methods = sol::state_view(state).registry()[methodsName];
                    sol::object forward = methods[key];
                    if (forward.get_type() == sol::type::function)
                        return forward;
                }

                auto component = ResolveComponent(ref, state);
                sol::stack::push(state, component);
                key.push(state);
                lua_gettable(state, -2);
                sol::stack_object value(state, lua_absindex(state, -1));
                if (value.get_type() != sol::type::function)
                    return value;

                auto registry = sol::state_view(state).registry();
                sol::table methods = registry[methodsName];
                sol::protected_function makeForward = registry[makeForwardName];
                sol::object forward = makeForward(value);
                methods[key] = forward;
                return forward;
            },
            sol::meta_function::new_index, [](const ComponentRef<Comp>& ref, sol::stack_object key, sol::stack_object value,
                sol::this_state state) {
                auto component = ResolveComponent(ref, state);
                sol::stack::push(state, component);
                key.push(state);
                value.push(state);
                lua_settable(state, -3);
                lua_pop(state, 1);
            },
            sol::meta_function::equal_to, [](const ComponentRef<Comp>& a, const ComponentRef<Comp>& b) {
                return a.entity == b.entity;
            }
        );

    }

    void GenerateSceneBindings(sol::table* ns) {

        ns->new_usertype<Scene::Wind>("Wind",
            "direction", &Scene::Wind::direction,
            "speed", &Scene::Wind::speed,
            "noiseMap", &Scene::Wind::noiseMap
            );

        ns->new_enum("SceneQueryComponents",
            "Mesh", Scene::SceneQueryComponentBits::MeshComponentBit,
            "RigidBody", Scene::SceneQueryComponentBits::RigidBodyComponentBit,
            "Text", Scene::SceneQueryComponentBits::TextComponentBit,
            "Terrain", Scene::SceneQueryComponentBits::TerrainComponentBit,
            "All", Scene::SceneQueryComponentBits::AllComponentsBit
            );

        ns->new_usertype<RayHit>("RayHit",
            "valid", &RayHit::valid,
            "hitDistance", &RayHit::hitDistance,
            "position", &RayHit::position,
            "normal", &RayHit::normal,
            "entity", &RayHit::entity
            );

        auto castRayOverload = sol::overload(
            [](Scene::Scene& scene, const Volume::Ray& ray) {
                return CastRay(scene, ray, Scene::SceneQueryComponentBits::AllComponentsBit, Scene::Entity());
            },
            [](Scene::Scene& scene, const Volume::Ray& ray, uint32_t components) {
                return CastRay(scene, ray, components, Scene::Entity());
            },
            [](Scene::Scene& scene, const Volume::Ray& ray, uint32_t components, Scene::Entity ignored) {
                return CastRay(scene, ray, components, ignored);
            }
        );

        // Entity lists are returned as tables of copies, elements of a bound vector would reference its storage
        auto queryAABBOverload = sol::overload(
            [](Scene::Scene& scene, const Volume::AABB& aabb) {
                return sol::as_table(scene.QueryAABB(aabb));
            },
            [](Scene::Scene& scene, const Volume::AABB& aabb, uint32_t components) {
                return sol::as_table(scene.QueryAABB(aabb, components));
            }
        );

        ns->new_usertype<Scene::Scene>("Scene",
            "CastRay", castRayOverload,
            "QueryAABB", queryAABBOverload,
            "CreateEntity", &Scene::Scene::CreateEntity,
            "DestroyEntity", &Scene::Scene::DestroyEntity,
            "DuplicateEntity", &Scene::Scene::DuplicateEntity,
            "GetEntityByName", &Scene::Scene::GetEntityByName,
            "GetParentEntity", &Scene::Scene::GetParentEntity,
            "GetGlobalMatrix", &Scene::Scene::GetGlobalMatrix,
            "GetEntityCount", &Scene::Scene::GetEntityCount,
            "GetMainCamera", [](Scene::Scene& scene, sol::this_state state) -> sol::object {
                // The main camera is a component as well, scripts get a reference to it
                auto& mainCamera = scene.GetMainCamera();
                auto cameraSubset = scene.GetSubset<CameraComponent>();
                for (auto entity : cameraSubset)
                    if (&cameraSubset.Get(entity) == &mainCamera)
                        return sol::make_object(state, ComponentRef<CameraComponent>{ entity });
                return sol::lua_nil;
            },
            "HasMainCamera", &Scene::Scene::HasMainCamera,
            "GetMeshes", &Scene::Scene::GetMeshes,
            "GetTime", &Scene::Scene::GetTime,
            "GetDeltaTime", &Scene::Scene::GetDeltaTime,
            "paused", &Scene::Scene::paused,
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
            "AddAnimationComponent", &AddComponentRef<AnimationComponent, ResourceHandle<Animation::Animation>&>,
            "AddAudioComponent", &AddComponentRef<AudioComponent, ResourceHandle<Audio::AudioData>&, float, bool>,
            "AddAudioVolumeComponent", &AddComponentRef<AudioVolumeComponent, ResourceHandle<Audio::AudioData>&, Volume::AABB&, float>,
            "AddCameraComponent", &AddComponentRef<CameraComponent, float, float, float, float, glm::vec3&, glm::vec2&>,
            "AddHierarchyComponent", &AddComponentRef<HierarchyComponent>,
            "AddLightComponent", &AddComponentRef<LightComponent>,
            "AddMeshComponent", &AddComponentRef<MeshComponent, ResourceHandle<Mesh::Mesh>&>,
            "AddNameComponent", &AddComponentRef<NameComponent, std::string>,
            "AddPlayerComponent", &AddComponentRef<PlayerComponent>,
            "AddRigidBodyComponent", &AddComponentRef<RigidBodyComponent, Physics::BodyCreationSettings&>,
            "AddTextComponent", &AddComponentRef<TextComponent, ResourceHandle<Font>&, std::string>,
            "AddTransformComponent", &AddComponentRef<TransformComponent, glm::mat4&, bool>,
            "AddLuaScriptComponent", &AddComponentRef<LuaScriptComponent, ResourceHandle<Script>&>,
            "AddSplineComponent", &AddComponentRef<SplineComponent>,

            // Remove components
            "RemoveAnimationComponent", &Scene::Entity::RemoveComponent<AnimationComponent>,
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
            "GetAnimationComponent", &GetComponentRef<AnimationComponent>,
            "GetAudioComponent", &GetComponentRef<AudioComponent>,
            "GetAudioVolumeComponent", &GetComponentRef<AudioVolumeComponent>,
            "GetCameraComponent", &GetComponentRef<CameraComponent>,
            "GetHierarchyComponent", &GetComponentRef<HierarchyComponent>,
            "GetLightComponent", &GetComponentRef<LightComponent>,
            "GetMeshComponent", &GetComponentRef<MeshComponent>,
            "GetNameComponent", &GetComponentRef<NameComponent>,
            "GetPlayerComponent", &GetComponentRef<PlayerComponent>,
            "GetRigidBodyComponent", &GetComponentRef<RigidBodyComponent>,
            "GetTextComponent", &GetComponentRef<TextComponent>,
            "GetTransformComponent", &GetComponentRef<TransformComponent>,
            "GetLuaScriptComponent", &GetComponentRef<LuaScriptComponent>,
            "GetSplineComponent", &GetComponentRef<SplineComponent>
        );

    }

    void GenerateComponentBindings(sol::table* ns) {

        auto playOverload = sol::overload(
            [](AnimationComponent& animationComponent, const ResourceHandle<Animation::Animation>& animation) {
                animationComponent.Play(animation);
            },
            [](AnimationComponent& animationComponent, const ResourceHandle<Animation::Animation>& animation, float blendTime) {
                animationComponent.Play(animation, blendTime);
            }
        );

        ns->new_usertype<AnimationComponent>("AnimationComponent",
            "Play", playOverload,
            "GetJointMatrix", &AnimationComponent::GetJointMatrix,
            "animation", &AnimationComponent::animation,
            "time", &AnimationComponent::time,
            "speed", &AnimationComponent::speed,
            "loop", &AnimationComponent::loop,
            "playing", &AnimationComponent::playing,
            "aabb", &AnimationComponent::aabb
        );

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
            "GetChildren", [](HierarchyComponent& component) { return sol::as_table(component.GetChildren()); },
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
            "SetGlobal", &TransformComponent::SetGlobal,
            "ReconstructLocalMatrix", &TransformComponent::ReconstructLocalMatrix,
            "matrix", &TransformComponent::matrix,
            "globalMatrix", &TransformComponent::globalMatrix,
            "isStatic", &TransformComponent::isStatic,
            "parentJoint", &TransformComponent::parentJoint
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

        GenerateComponentRefBindings<AnimationComponent>(ns, "AnimationComponent");
        GenerateComponentRefBindings<AudioComponent>(ns, "AudioComponent");
        GenerateComponentRefBindings<AudioVolumeComponent>(ns, "AudioVolumeComponent");
        GenerateComponentRefBindings<CameraComponent>(ns, "CameraComponent");
        GenerateComponentRefBindings<HierarchyComponent>(ns, "HierarchyComponent");
        GenerateComponentRefBindings<LightComponent>(ns, "LightComponent");
        GenerateComponentRefBindings<MeshComponent>(ns, "MeshComponent");
        GenerateComponentRefBindings<NameComponent>(ns, "NameComponent");
        GenerateComponentRefBindings<PlayerComponent>(ns, "PlayerComponent");
        GenerateComponentRefBindings<RigidBodyComponent>(ns, "RigidBodyComponent");
        GenerateComponentRefBindings<TextComponent>(ns, "TextComponent");
        GenerateComponentRefBindings<TransformComponent>(ns, "TransformComponent");
        GenerateComponentRefBindings<LuaScriptComponent>(ns, "LuaScriptComponent");
        GenerateComponentRefBindings<SplineComponent>(ns, "SplineComponent");

    }

}
