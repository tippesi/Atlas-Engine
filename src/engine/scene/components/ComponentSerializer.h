#pragma once

#include "Components.h"
#include "LuaScriptComponent.h"

#include "common/SerializationHelper.h"
#include "lighting/LightingSerializer.h"
#include "audio/AudioSerializer.h"
#include "physics/PhysicsSerializer.h"

#include "resource/ResourceManager.h"
#include "audio/AudioManager.h"
#include "loader/MeshDataLoader.h"

namespace Atlas::Scene::Components {

    void to_json(json& j, const AudioComponent& p);

    void from_json(const json& j, AudioComponent& p);

    void to_json(json& j, const AudioVolumeComponent& p);

    void from_json(const json& j, AudioVolumeComponent& p);

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

    void to_json(json& j, const TextComponent& p);

    void from_json(const json& j, TextComponent& p);

    void to_json(json& j, const RigidBodyComponent& p);

    void from_json(const json& j, RigidBodyComponent& p);

    void to_json(json& j, const PlayerComponent& p);

    void from_json(const json& j, PlayerComponent& p);

	void to_json(json& j, const LuaScriptComponent& p);

    void from_json(const json& j, LuaScriptComponent& p);

}