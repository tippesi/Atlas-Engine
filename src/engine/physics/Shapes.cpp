#include "Shapes.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

namespace Atlas {

    namespace Physics {

        using namespace JPH;

        Shape::Shape(ResourceHandle<Mesh::Mesh> mesh) : mesh(mesh) {

            if (!mesh.IsLoaded())
                return;

        }

        Shape::Shape(float radius) {

            SphereShapeSettings sphereShapeSettings(radius);

            shapeRef = sphereShapeSettings.Create().Get();

        }

        bool Shape::TryCreateShape() {

            if (shapeRef.GetPtr() != nullptr) return true;

            if (!mesh.IsValid() || !mesh.IsLoaded())
                return false;

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

            return true;

        }

    }

}