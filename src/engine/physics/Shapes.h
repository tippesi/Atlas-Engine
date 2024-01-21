#pragma once

#include "../System.h"

#include "../mesh/Mesh.h"
#include "../volume/AABB.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace Atlas {

    namespace Physics {

        using ShapeRef = JPH::ShapeRefC;

        class ShapesManager {

        public:
            explicit ShapesManager(Ref<Mesh::Mesh> mesh);

            explicit ShapesManager(const Volume::AABB& aabb, float density = 1.0f);

            explicit ShapesManager(float radius, float density = 1.0f);

            bool TryCreateShape();

            ShapeRef shapeRef;

        };

    }

}