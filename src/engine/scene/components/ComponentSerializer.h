#pragma once

#include "Components.h"
#include "LuaScriptComponent.h"

#include "common/SerializationHelper.h"

#include <functional>

namespace Atlas::Scene::Components {

    void to_json(json& j, const AnimationComponent& p);

    void from_json(const json& j, AnimationComponent& p);

    void to_json(json& j, const AudioComponent& p);

    void from_json(const json& j, AudioComponent& p);

    void to_json(json& j, const AudioVolumeComponent& p);

    void from_json(const json& j, AudioVolumeComponent& p);

    void to_json(json& j, const FogVolumeComponent& p);

    void from_json(const json& j, FogVolumeComponent& p);

    void to_json(json& j, const CameraComponent& p);

    void from_json(const json& j, CameraComponent& p);

    void to_json(json& j, const LightComponent& p);

    void from_json(const json& j, LightComponent& p);

    void to_json(json& j, const MeshComponent& p);

    void from_json(const json& j, MeshComponent& p);

    void to_json(json& j, const NameComponent& p);

    void from_json(const json& j, NameComponent& p);

    void to_json(json& j, const TransformComponent& p);

    void from_json(const json& j, TransformComponent& p);

    /**
     * Binary files of scenes with binary columns store a component type as columns of raw values instead of a json
     * object per component. ToValues converts the columns into the json objects of a binary scene without columns,
     * e.g. for tools reading the file.
     */
    template<class T>
    struct ComponentColumns {
        static constexpr bool available = false;
    };

    template<>
    struct ComponentColumns<NameComponent> {
        static constexpr bool available = true;
        static void Write(json& j, size_t count, const std::function<const NameComponent&(size_t)>& get);
        static bool Read(const json& j, std::vector<NameComponent>& components);
        static json ToValues(const json& j);
    };

    template<>
    struct ComponentColumns<TransformComponent> {
        static constexpr bool available = true;
        static void Write(json& j, size_t count, const std::function<const TransformComponent&(size_t)>& get);
        static bool Read(const json& j, std::vector<TransformComponent>& components);
        static json ToValues(const json& j);
    };

    template<>
    struct ComponentColumns<MeshComponent> {
        static constexpr bool available = true;
        static void Write(json& j, size_t count, const std::function<const MeshComponent&(size_t)>& get);
        static bool Read(const json& j, std::vector<MeshComponent>& components);
        static json ToValues(const json& j);
    };

    void to_json(json& j, const TextComponent& p);

    void from_json(const json& j, TextComponent& p);

    void to_json(json& j, const RigidBodyComponent& p);

    void from_json(const json& j, RigidBodyComponent& p);

    void to_json(json& j, const PlayerComponent& p);

    void from_json(const json& j, PlayerComponent& p);

    void to_json(json& j, const ConstraintComponent& p);

    void from_json(const json& j, ConstraintComponent& p);

	void to_json(json& j, const LuaScriptComponent& p);

    void from_json(const json& j, LuaScriptComponent& p);

    void to_json(json& j, const SplineComponent& p);

    void from_json(const json& j, SplineComponent& p);

}