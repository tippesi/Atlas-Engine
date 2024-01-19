#include "Shapes.h"
#include "MathConversion.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

namespace Atlas {

    namespace Physics {

        using namespace JPH;

        Shape::Shape(Ref<Mesh::Mesh> mesh) {

            VertexList vertexList;
            IndexedTriangleList triangleList;

            for (auto& vertex : mesh->data.vertices) {

                vertexList.push_back(Float3(vertex.x, vertex.y, vertex.z));

            }

            for (size_t i = 0; i < mesh->data.indices.GetElementCount(); i+=3) {

                auto idx0 = mesh->data.indices[i];
                auto idx1 = mesh->data.indices[i + 1];
                auto idx2 = mesh->data.indices[i + 2];

                IndexedTriangle triangle(idx0, idx1, idx2);

                triangleList.push_back(triangle);
            }

            MeshShapeSettings meshShapeSettings(vertexList, triangleList);

            shapeRef = meshShapeSettings.Create().Get();

        }

        Shape::Shape(const Volume::AABB& aabb, float density) {

            auto halfSize = aabb.GetSize() / 2.0f;

            BoxShapeSettings boxShapeSettings(VecToJPHVec(halfSize));
            boxShapeSettings.SetDensity(density);

            auto boxShapeRef = boxShapeSettings.Create().Get();

            auto translation = VecToJPHVec(aabb.min + halfSize);
            JPH::RotatedTranslatedShapeSettings translatedBox(translation, JPH::Quat::sIdentity(), boxShapeRef);
            shapeRef = translatedBox.Create().Get();

        }

        Shape::Shape(float radius, float density) {

            SphereShapeSettings sphereShapeSettings(radius);
            sphereShapeSettings.SetDensity(density);

            shapeRef = sphereShapeSettings.Create().Get();

        }

        bool Shape::TryCreateShape() {

            return shapeRef.GetPtr() != nullptr;

        }

    }

}