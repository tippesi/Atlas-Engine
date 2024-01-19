#pragma once

#include "../System.h"

#include "../mesh/Mesh.h"
#include "../volume/AABB.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace Atlas {

    namespace Physics {

        class Shape {

        public:
            explicit Shape(Ref<Mesh::Mesh> mesh);

            explicit Shape(const Volume::AABB& aabb, float density = 1.0f);

            explicit Shape(float radius, float density = 1.0f);

            bool TryCreateShape();

            JPH::ShapeRefC shapeRef;

        };

    }

}