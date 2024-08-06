#include "PhysicsSerializer.h"

#include <Jolt/Physics/PhysicsScene.h>
#include <Jolt/ObjectStream/ObjectStreamTextIn.h>
#include <Jolt/ObjectStream/ObjectStreamTextOut.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>

namespace Atlas::Physics {

    void to_json(json& j, const MeshShapeSettings& p) {
        j = json {
            {"scale", p.scale},
        };

        if (p.mesh.IsValid()) {
            j["resourcePath"] = p.mesh.GetResource()->path;
        }
    }

    void from_json(const json& j, MeshShapeSettings& p) {
        j.at("scale").get_to(p.scale);

        if (j.contains("resourcePath")) {
            std::string resourcePath;
            j.at("resourcePath").get_to(resourcePath);

            p.mesh = ResourceManager<Mesh::Mesh>::GetOrLoadResourceWithLoaderAsync(resourcePath,
                ResourceOrigin::User, Loader::ModelImporter::ImportMesh, false, 8192);
        }
    }

    void to_json(json& j, const SphereShapeSettings& p) {
        j = json {
            {"radius", p.radius},
            {"density", p.density},
            {"scale", p.scale},
        };
    }

    void from_json(const json& j, SphereShapeSettings& p) {
        j.at("radius").get_to(p.radius);
        j.at("density").get_to(p.density);
        j.at("scale").get_to(p.scale);
    }

    void to_json(json& j, const BoundingBoxShapeSettings& p) {
        j = json {
            {"aabb", p.aabb},
            {"density", p.density},
            {"scale", p.scale},
        };
    }

    void from_json(const json& j, BoundingBoxShapeSettings& p) {
        j.at("aabb").get_to(p.aabb);
        j.at("density").get_to(p.density);
        j.at("scale").get_to(p.scale);
    }

    void to_json(json& j, const CapsuleShapeSettings& p) {
        j = json{
            {"height", p.height},
            {"radius", p.radius},
            {"density", p.density},
            {"scale", p.scale},
        };
    }

    void from_json(const json& j, CapsuleShapeSettings& p) {
        j.at("height").get_to(p.height);
        j.at("radius").get_to(p.radius);
        j.at("density").get_to(p.density);
        j.at("scale").get_to(p.scale);
    }

    void to_json(json& j, const HeightFieldShapeSettings& p) {
        j = json {
            {"heightData", p.heightData},
            {"translation", p.translation},
            {"scale", p.scale},
        };
    }

    void from_json(const json& j, HeightFieldShapeSettings& p) {
        j.at("heightData").get_to(p.heightData);
        j.at("translation").get_to(p.translation);
        j.at("scale").get_to(p.scale);
    }

    void to_json(json& j, const Shape& p) {
        if (p.type == ShapeType::Mesh) {
            auto meshSettings = static_cast<MeshShapeSettings*>(p.settings.get());
            j["meshSettings"] = *meshSettings;
        }
        else if (p.type == ShapeType::Sphere) {
            auto sphereSettings = static_cast<SphereShapeSettings*>(p.settings.get());
            j["sphereSettings"] = *sphereSettings;
        }
        else if (p.type == ShapeType::BoundingBox) {
            auto boundingBoxSettings = static_cast<BoundingBoxShapeSettings*>(p.settings.get());
            j["boundingBoxSettings"] = *boundingBoxSettings;
        }
        else if (p.type == ShapeType::Capsule) {
            auto capsuleShapeSettings = static_cast<CapsuleShapeSettings*>(p.settings.get());
            j["capsuleShapeSettings"] = *capsuleShapeSettings;
        }
        else if (p.type == ShapeType::HeightField) {
            auto heightFieldSettings = static_cast<HeightFieldShapeSettings*>(p.settings.get());
            j["heightFieldSettings"] = *heightFieldSettings;
        }
    }

    void from_json(const json& j, Ref<Shape>& p) {
        if (j.contains("meshSettings")) {
            MeshShapeSettings settings = j["meshSettings"];
            p = ShapesManager::CreateShape(settings);
        }
        else if (j.contains("sphereSettings")) {
            SphereShapeSettings settings = j["sphereSettings"];
            p = ShapesManager::CreateShape(settings);
        }
        else if (j.contains("boundingBoxSettings")) {
            BoundingBoxShapeSettings settings = j["boundingBoxSettings"];
            p = ShapesManager::CreateShape(settings);
        }
        else if (j.contains("capsuleShapeSettings")) {
            CapsuleShapeSettings settings = j["capsuleShapeSettings"];
            p = ShapesManager::CreateShape(settings);
        }
        else if (j.contains("heightFieldSettings")) {
            HeightFieldShapeSettings settings = j["heightFieldSettings"];
            p = ShapesManager::CreateShape(settings);
        }
    }

    void to_json(json& j, const BodyCreationSettings& p) {
        // Keep default value and compare, no need to write everything
        BodyCreationSettings d;
        if (d.objectLayer != p.objectLayer)
            j["objectLayer"] = p.objectLayer;
        if (d.motionQuality != p.motionQuality)
            j["motionQuality"] = p.motionQuality;
        if (d.linearVelocity != p.linearVelocity)
            j["linearVelocity"] = p.linearVelocity;
        if (d.angularVelocity != p.angularVelocity)
            j["angularVelocity"] = p.angularVelocity;
        if (d.friction != p.friction)
            j["friction"] = p.friction;
        if (d.restitution != p.restitution)
            j["restitution"] = p.restitution;
        if (d.linearDampening != p.linearDampening)
            j["linearDampening"] = p.linearDampening;
        if (d.angularDampening != p.angularDampening)
            j["angularDampening"] = p.angularDampening;
        if (d.gravityFactor != p.gravityFactor)
            j["gravityFactor"] = p.gravityFactor;
        if (p.shape)
            j["shape"] = *p.shape;
    }

    void from_json(const json& j, BodyCreationSettings& p) {
        if (j.contains("objectLayer"))
            p.objectLayer = j["objectLayer"];
        if (j.contains("motionQuality"))
            p.motionQuality = j["motionQuality"];
        if (j.contains("linearVelocity"))
            p.linearVelocity = j["linearVelocity"];
        if (j.contains("angularVelocity"))
            p.angularVelocity = j["angularVelocity"];
        if (j.contains("friction"))
            p.friction = j["friction"];
        if (j.contains("restitution"))
            p.restitution = j["restitution"];
        if (j.contains("linearDampening"))
            p.linearDampening = j["linearDampening"];
        if (j.contains("angularDampening"))
            p.angularDampening = j["angularDampening"];
        if (j.contains("gravityFactor"))
            p.gravityFactor = j["gravityFactor"];
        if (j.contains("shape")) {
            p.shape = j["shape"];
        }
    }

    void to_json(json& j, const PlayerCreationSettings& p) {
        // Keep default value and compare, no need to write everything
        PlayerCreationSettings d;
        if (d.maxSlopeAngle != p.maxSlopeAngle)
            j["maxSlopeAngle"] = p.maxSlopeAngle;
        if (d.up != p.up)
            j["up"] = p.up;
        if (d.mass != p.mass)
            j["mass"] = p.mass;
        if (d.maxStrength != p.maxStrength)
            j["maxStrength"] = p.maxStrength;
        if (d.predictiveContactDistance != p.predictiveContactDistance)
            j["predictiveContactDistance"] = p.predictiveContactDistance;
        if (d.shapePadding != p.shapePadding)
            j["shapePadding"] = p.shapePadding;
        if (d.shapeOffset != p.shapeOffset)
            j["shapeOffset"] = p.shapeOffset;
        if (p.shape)
            j["shape"] = *p.shape;
    }

    void from_json(const json& j, PlayerCreationSettings& p) {
        if (j.contains("maxSlopeAngle"))
            p.maxSlopeAngle = j["maxSlopeAngle"];
        if (j.contains("up"))
            p.up = j["up"];
        if (j.contains("mass"))
            p.mass = j["mass"];
        if (j.contains("maxStrength"))
            p.maxStrength = j["maxStrength"];
        if (j.contains("predictiveContactDistance"))
            p.predictiveContactDistance = j["predictiveContactDistance"];
        if (j.contains("shapePadding"))
            p.shapePadding = j["shapePadding"];
        if (j.contains("shapeOffset"))
            p.shapeOffset = j["shapeOffset"];
        if (j.contains("shape")) {
            p.shape = j["shape"];
        }
    }

    void SerializePhysicsWorld(json& j, Ref<PhysicsWorld>& physicsWorld) {

        /*
        auto& system = physicsWorld->system;
        auto& bodyInterface = system->GetBodyInterface();

        JPH::BodyIDVector bodyIds;
        system->GetBodies(bodyIds);

        auto& bodyLockInterface = system->GetBodyLockInterface();

        std::unordered_map<uint32_t, BodyCreationSettings> bodyCreationSettings;
       
        for (auto bodyId : bodyIds) {
            JPH::BodyLockRead lock(bodyLockInterface, bodyId);
            if (lock.Succeeded()) {
                const auto& body = lock.GetBody();

                if (body.IsRigidBody()) {
                    JPH::BodyCreationSettings settings = body.GetBodyCreationSettings();
                    BodyCreationSettings creationSettings;
                    creationSettings.SetSettings(settings);
                    creationSettings.shape = physicsWorld->bodyToShapeMap[bodyId];
                    bodyCreationSettings[bodyId.GetIndex()] = creationSettings;
                }
                else {
                    JPH::SoftBodyCreationSettings settings = body.GetSoftBodyCreationSettings();
                }
            }
        }

        j["bodies"] = bodyCreationSettings;
        */

    }

    void DeserializePhysicsWorld(const json& j, std::unordered_map<uint32_t, BodyCreationSettings>& bodyCreationMap) {

        // Bodies are now saved in the enities
        // bodyCreationMap = j["bodies"];

    }

}