#pragma once

#include "../System.h"

#include "../mesh/Mesh.h"
#include "../resource/Resource.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace Atlas {

    namespace Physics {

        class Shape {

        public:
            Shape(ResourceHandle<Mesh::Mesh> mesh);

            Shape(float radius);

            bool TryCreateShape();

            JPH::ShapeRefC shapeRef;

        private:
            ResourceHandle<Mesh::Mesh> mesh;

        };

    }

}