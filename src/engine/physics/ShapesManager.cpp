#include "ShapesManager.h"
#include "MathConversion.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>

namespace Atlas {

    namespace Physics {

        using namespace JPH;

        ShapeRef ShapesManager::CreateShapeFromMesh(Ref<Mesh::Mesh> mesh, vec3 scale) {

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
            auto result = meshShapeSettings.Create();

            if (!result.IsValid())
                return nullptr;

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return CreateShapeScaled(result.Get(), scale);
            }

            return result.Get();

        }

        ShapeRef ShapesManager::CreateShapeFromAABB(const Volume::AABB &aabb, vec3 scale, float density) {

            auto halfSize = aabb.GetSize() / 2.0f;

            BoxShapeSettings boxShapeSettings(VecToJPHVec(halfSize));
            boxShapeSettings.SetDensity(density);

            auto boxShapeRef = boxShapeSettings.Create().Get();

            auto translation = VecToJPHVec(aabb.min + halfSize);
            JPH::RotatedTranslatedShapeSettings translatedBox(translation, JPH::Quat::sIdentity(), boxShapeRef);

            auto result = translatedBox.Create();

            if (!result.IsValid())
                return nullptr;

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return CreateShapeScaled(result.Get(), scale);
            }

            return result.Get();

        }

        ShapeRef ShapesManager::CreateShapeFromSphere(float radius, vec3 scale, float density) {

            SphereShapeSettings sphereShapeSettings(radius);
            sphereShapeSettings.SetDensity(density);

            auto result = sphereShapeSettings.Create();

            if (!result.IsValid())
                return nullptr;

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return CreateShapeScaled(result.Get(), scale);
            }

            return result.Get();

        }

        ShapeRef ShapesManager::CreateShapeFromHeightField(std::vector<float> &heightData, glm::vec3 translation,
            glm::vec3 scale) {

            auto sampleCount = std::sqrt(heightData.size());

            HeightFieldShapeSettings heightFieldShapeSettings(heightData.data(), VecToJPHVec(translation),
                VecToJPHVec(scale), uint32_t(sampleCount));

            auto result = heightFieldShapeSettings.Create();

            if (!result.IsValid())
                return nullptr;

            return result.Get();

        }

        ShapeRef ShapesManager::CreateShapeScaled(ShapeRef shape, vec3 scale) {

            ScaledShapeSettings scaledShapeSettings(shape, VecToJPHVec(scale));
            auto result = scaledShapeSettings.Create();

            if (!result.IsValid())
                return nullptr;

            return result.Get();

        }

    }

}