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
            static ShapeRef CreateShapeFromMesh(Ref<Mesh::Mesh> mesh, vec3 scale = vec3(1.0f));

            static ShapeRef CreateShapeFromAABB(const Volume::AABB& aabb, vec3 scale = vec3(1.0f), float density = 1.0f);

            static ShapeRef CreateShapeFromSphere(float radius, vec3 scale = vec3(1.0f), float density = 1.0f);

        private:
            static ShapeRef CreateShapeScaled(ShapeRef shape, vec3 scale);

        };

    }

}