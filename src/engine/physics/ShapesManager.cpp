#include "ShapesManager.h"
#include "MathConversion.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>

namespace Atlas {

    namespace Physics {

        bool ShapesManager::TryCreateShapeFromMesh(Shape* shape, const MeshShapeSettings& settings) {

            if (!settings.mesh.IsLoaded())
                return false;

            auto& mesh = settings.mesh;
            const auto& scale = settings.scale;

            JPH::VertexList vertexList;
            JPH::IndexedTriangleList triangleList;

            for (auto& vertex : mesh->data.vertices) {

                vertexList.push_back(JPH::Float3(vertex.x, vertex.y, vertex.z));

            }

            for (size_t i = 0; i < mesh->data.indices.GetElementCount(); i+=3) {

                auto idx0 = mesh->data.indices[i];
                auto idx1 = mesh->data.indices[i + 1];
                auto idx2 = mesh->data.indices[i + 2];

                JPH::IndexedTriangle triangle(idx0, idx1, idx2);

                triangleList.push_back(triangle);
            }

            JPH::MeshShapeSettings meshShapeSettings(vertexList, triangleList);
            auto result = meshShapeSettings.Create();

            if (!result.IsValid())
                return false;

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return CreateShapeScaled(shape, result.Get(), scale);
            }

            shape->ref = result.Get();

            return true;

        }

        bool ShapesManager::TryCreateShapeFromAABB(Shape* shape, const BoundingBoxShapeSettings& settings) {

            const auto& aabb = settings.aabb;
            auto density = settings.density;
            const auto& scale = settings.scale;

            auto halfSize = aabb.GetSize() / 2.0f;

            JPH::BoxShapeSettings boxShapeSettings(VecToJPHVec(halfSize));
            boxShapeSettings.SetDensity(density);

            auto boxShapeResult = boxShapeSettings.Create();
            if (!boxShapeResult.IsValid()) {
                auto error = boxShapeResult.GetError();
                return false;
            }

            auto boxShapeRef = boxShapeResult.Get();

            auto translation = VecToJPHVec(aabb.min + halfSize);
            JPH::RotatedTranslatedShapeSettings translatedBox(translation, JPH::Quat::sIdentity(), boxShapeRef);

            auto result = translatedBox.Create();

            if (!result.IsValid())
                return false;

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return CreateShapeScaled(shape, result.Get(), scale);
            }

            shape->ref = result.Get();

            return true;

        }

        bool ShapesManager::TryCreateShapeFromSphere(Shape* shape, const SphereShapeSettings& settings) {

            auto radius = settings.radius;
            auto density = settings.density;
            const auto& scale = settings.scale;

            JPH::SphereShapeSettings sphereShapeSettings(radius);
            sphereShapeSettings.SetDensity(density);

            auto result = sphereShapeSettings.Create();

            if (!result.IsValid())
                return false;

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return CreateShapeScaled(shape, result.Get(), scale);
            }

            shape->ref = result.Get();

            return true;

        }

        bool ShapesManager::TryCreateShapeFromCapsule(Shape* shape, const CapsuleShapeSettings& settings) {

            auto halfHeight = settings.height / 2.0f;
            auto radius = settings.radius;
            auto density = settings.density;
            const auto& scale = settings.scale;

            JPH::CapsuleShapeSettings capsuleShapeSettings(halfHeight, radius);
            capsuleShapeSettings.SetDensity(density);

            auto capsuleResult = capsuleShapeSettings.Create();

            if (!capsuleResult.IsValid())
                return false;

            auto capsuleRef = capsuleResult.Get();

            auto translation = VecToJPHVec(vec3(0.0f, halfHeight + radius, 0.0f));
            JPH::RotatedTranslatedShapeSettings translatedCapsule(translation, JPH::Quat::sIdentity(), capsuleRef);

            auto result = translatedCapsule.Create();

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return CreateShapeScaled(shape, result.Get(), scale);
            }

            shape->ref = result.Get();

            return true;

        }

        bool ShapesManager::TryCreateShapeFromHeightField(Shape* shape, const HeightFieldShapeSettings& settings) {

            const auto& heightData = settings.heightData;
            const auto& translation = settings.translation;
            const auto& scale = settings.scale;

            auto sampleCount = std::sqrt(heightData.size());

            JPH::HeightFieldShapeSettings heightFieldShapeSettings(heightData.data(), VecToJPHVec(translation),
                VecToJPHVec(scale), uint32_t(sampleCount));

            auto result = heightFieldShapeSettings.Create();

            if (!result.IsValid())
                return false;

            shape->ref = result.Get();

            return true;

        }

        bool ShapesManager::CreateShapeScaled(Shape* shape, ShapeRef shapeRef, vec3 scale) {

            JPH::ScaledShapeSettings scaledShapeSettings(shapeRef, VecToJPHVec(scale));
            auto result = scaledShapeSettings.Create();

            if (!result.IsValid())
                return false;

            shape->ref = result.Get();

            return true;

        }

    }

}