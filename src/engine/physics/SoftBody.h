#pragma once

#include "System.h"
#include "InterfaceImplementations.h"
#include "BodyCreation.h"
#include "volume/AABB.h"

#include <vector>

namespace Atlas::Physics {

    enum class SoftBodyBendType {
        None = 0,
        Distance,
        Dihedral
    };

    struct SoftBodySettings {
        // Spread over all vertices which aren't pinned by the area around them
        float mass = 1.0f;

        // Inverse stiffness of the constraints, 0 is rigid
        float edgeCompliance = 0.0f;
        float shearCompliance = 0.0f;
        float bendCompliance = 0.001f;
        SoftBodyBendType bendType = SoftBodyBendType::Distance;

        // Inflates closed meshes like balloons
        float pressure = 0.0f;

        int32_t iterations = 5;
        float linearDamping = 0.1f;
        float friction = 0.2f;
        float restitution = 0.0f;
        float gravityFactor = 1.0f;
        // Keeps the vertices a little away from other bodies
        float vertexRadius = 0.0f;
        bool doubleSided = true;
        bool allowSleeping = true;

        // Drag towards the scene wind: the share of the difference to the wind velocity the free vertices take on per
        // second, 0 leaves them to gravity
        float windFactor = 0.0f;
        // Simulated within this distance of the main camera only (0 always), farther away the mesh rests in its rest pose
        float simulationDistance = 0.0f;

        ObjectLayer objectLayer = Layers::Movable;

        // Vertices within any of the pin bounds (mesh space) follow the transform of the entity
        bool pinned = true;
        std::vector<Volume::AABB> pinBounds;
    };

    // Welded vertices in the space of the body. Inverse masses are relative and scaled to the mass of the body, pinned
    // vertices have zero
    struct SoftBodyGeometry {
        std::vector<vec3> vertices;
        std::vector<float> inverseMasses;
        std::vector<uint32_t> indices;
    };

}
