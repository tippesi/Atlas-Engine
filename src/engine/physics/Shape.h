#pragma once

#include "System.h"

#include "common/Hash.h"

#include "../mesh/Mesh.h"
#include "../volume/AABB.h"
#include "../resource/Resource.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace Atlas::Physics {

    enum class ShapeType {
        Mesh = 0,
        Sphere,
        BoundingBox,
        HeightField
    };

    struct ShapeSettings {};

    struct MeshShapeSettings : ShapeSettings {
        ResourceHandle<Mesh::Mesh> mesh;
        vec3 scale = vec3(1.0f);
    };

    struct SphereShapeSettings : ShapeSettings {
        float radius = 1.0f;
        float density = 1.0f;
        vec3 scale = vec3(1.0f);
    };

    struct BoundingBoxShapeSettings : ShapeSettings {
        Volume::AABB aabb;
        float density = 1.0f;
        vec3 scale = vec3(1.0f);
    };

    struct HeightFieldShapeSettings : ShapeSettings {
        std::vector<float> heightData;
        vec3 translation = vec3(0.0f);
        vec3 scale = vec3(1.0f);
    };

    using ShapeRef = JPH::ShapeRefC;

    // Serialize by using the type + the corresponding shape settings
    class Shape {

    public:
        bool IsValid() const { return ref != nullptr; }

        bool TryCreate();

        bool Scale(vec3 scale);

        ShapeRef ref = nullptr;

        ShapeType type;
        Ref<ShapeSettings> settings = nullptr;
    };


}