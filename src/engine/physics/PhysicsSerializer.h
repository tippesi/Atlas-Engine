#pragma once

#include "PhysicsWorld.h"

#include "common/SerializationHelper.h"
#include "loader/ModelLoader.h"

#include <string>
#include <unordered_map>

namespace Atlas::Physics {

    void to_json(json& j, const MeshShapeSettings& p);

    void from_json(const json& j, MeshShapeSettings& p);

    void to_json(json& j, const SphereShapeSettings& p);

    void from_json(const json& j, SphereShapeSettings& p);

    void to_json(json& j, const BoundingBoxShapeSettings& p);

    void from_json(const json& j, BoundingBoxShapeSettings& p);

    void to_json(json& j, const CapsuleShapeSettings& p);

    void from_json(const json& j, CapsuleShapeSettings& p);

    void to_json(json& j, const HeightFieldShapeSettings& p);

    void from_json(const json& j, HeightFieldShapeSettings& p);

    void to_json(json& j, const Shape& p);

    void from_json(const json& j, Ref<Shape>& p);

    void to_json(json& j, const BodyCreationSettings& p);

    void from_json(const json& j, BodyCreationSettings& p);

    void to_json(json& j, const PlayerCreationSettings& p);

    void from_json(const json& j, PlayerCreationSettings& p);

    void SerializePhysicsWorld(json& j, Ref<PhysicsWorld>& physicsWorld) ;

    void DeserializePhysicsWorld(const json& j, std::unordered_map<uint32_t, BodyCreationSettings>& bodyCreationMap);

}