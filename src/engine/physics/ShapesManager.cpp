#include "ShapesManager.h"
#include "MathConversion.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <glm/gtx/component_wise.hpp>

namespace Atlas {

    namespace Physics {

        std::unordered_map<Hash, ShapesManager::CacheItem<Mesh::Mesh>> ShapesManager::meshShapeCache;
        std::mutex ShapesManager::meshShapeCacheMutex;

        std::unordered_map<Hash, ShapeRef> ShapesManager::heightFieldShapeCache;
        std::mutex ShapesManager::heightFieldShapeCacheMutex;

        std::unordered_map<Hash, ShapesManager::CacheItem<Mesh::Mesh>> ShapesManager::convexHullShapeCache;
        std::mutex ShapesManager::convexHullShapeCacheMutex;

        const float ShapesManager::epsilon = 0.00001f;

        bool ShapesManager::TryCreateShapeFromMesh(Shape* shape, const MeshShapeSettings& settings) {

            if (!settings.mesh.IsLoaded())
                return false;

            auto& mesh = settings.mesh;
            const auto& scale = settings.scale;

            // This all assumes right now that mesh data doesn't change in the objects lifetime
            meshShapeCacheMutex.lock();
            if (!meshShapeCache.contains(mesh.GetID())) {
                meshShapeCacheMutex.unlock();

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

                shape->ref = result.Get();

                meshShapeCacheMutex.lock();
                meshShapeCache[mesh.GetID()] = { mesh.Get(), shape->ref };

            }
            else {

                shape->ref = meshShapeCache[mesh.GetID()].ref;

            }
            meshShapeCacheMutex.unlock();

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return shape->Scale(scale);
            }

            return true;

        }

        bool ShapesManager::TryCreateShapeFromAABB(Shape* shape, const BoundingBoxShapeSettings& settings) {

            const auto& aabb = settings.aabb;
            auto density = settings.density;
            const auto& scale = settings.scale;

            auto halfSize = aabb.GetSize() / 2.0f;

            // 0.05f is the default convex radius of Jolt
            auto convexRadius = glm::min(glm::compMin(halfSize) - epsilon, 0.05f);

            JPH::BoxShapeSettings boxShapeSettings(VecToJPHVec(halfSize), convexRadius);
            boxShapeSettings.SetDensity(density);

            auto boxShapeResult = boxShapeSettings.Create();
            if (!boxShapeResult.IsValid()) 
                return false;

            auto boxShapeRef = boxShapeResult.Get();

            auto translation = VecToJPHVec(aabb.min + halfSize);
            JPH::RotatedTranslatedShapeSettings translatedBox(translation, JPH::Quat::sIdentity(), boxShapeRef);

            auto result = translatedBox.Create();

            if (!result.IsValid())
                return false;

            shape->ref = result.Get();

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return shape->Scale(scale);
            }

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

            shape->ref = result.Get();

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return shape->Scale(scale);
            }

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
            if (!result.IsValid())
                return false;

            shape->ref = result.Get();

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return shape->Scale(scale);
            }

            return true;

        }

        bool ShapesManager::TryCreateShapeFromHeightField(Shape* shape, const HeightFieldShapeSettings& settings) {

            const auto& heightData = settings.heightData;
            const auto& translation = settings.translation;
            const auto& scale = settings.scale;

            // Restoring a scene backup recreates the same height field, building it again is expensive
            Hash hash = std::hash<std::string_view>()(std::string_view(
                reinterpret_cast<const char*>(heightData.data()), heightData.size() * sizeof(float)));
            for (int32_t i = 0; i < 3; i++) {
                HashCombine(hash, translation[i]);
                HashCombine(hash, scale[i]);
            }

            {
                std::scoped_lock lock(heightFieldShapeCacheMutex);
                if (heightFieldShapeCache.contains(hash)) {
                    shape->ref = heightFieldShapeCache[hash];
                    return true;
                }
            }

            auto sampleCount = std::sqrt(heightData.size());

            JPH::HeightFieldShapeSettings heightFieldShapeSettings(heightData.data(), VecToJPHVec(translation),
                VecToJPHVec(scale), uint32_t(sampleCount));

            auto result = heightFieldShapeSettings.Create();

            if (!result.IsValid())
                return false;

            shape->ref = result.Get();

            std::scoped_lock lock(heightFieldShapeCacheMutex);
            heightFieldShapeCache[hash] = shape->ref;

            return true;

        }

        bool ShapesManager::TryCreateShapeFromConvexHull(Shape* shape, const ConvexHullShapeSettings& settings) {

            if (!settings.mesh.IsLoaded())
                return false;

            auto& mesh = settings.mesh;
            const auto& scale = settings.scale;

            // The density is part of the shape, hulls of the same mesh can only be shared with the same density
            Hash hash = mesh.GetID();
            HashCombine(hash, settings.density);

            ShapeRef ref = nullptr;
            {
                std::scoped_lock lock(convexHullShapeCacheMutex);
                if (convexHullShapeCache.contains(hash))
                    ref = convexHullShapeCache[hash].ref;
            }

            if (ref == nullptr) {
                JPH::Array<JPH::Vec3> points;
                points.reserve(mesh->data.vertices.GetElementCount());
                for (auto& vertex : mesh->data.vertices)
                    points.push_back(JPH::Vec3(vertex.x, vertex.y, vertex.z));

                JPH::ConvexHullShapeSettings convexHullShapeSettings(points);
                convexHullShapeSettings.SetDensity(settings.density);

                auto result = convexHullShapeSettings.Create();

                if (!result.IsValid())
                    return false;

                ref = result.Get();

                std::scoped_lock lock(convexHullShapeCacheMutex);
                convexHullShapeCache[hash] = { mesh.Get(), ref };
            }

            shape->ref = ref;

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return shape->Scale(scale);
            }

            return true;

        }

        bool ShapesManager::TryCreateShapeFromCylinder(Shape* shape, const CylinderShapeSettings& settings) {

            auto halfHeight = settings.height / 2.0f;
            auto radius = settings.radius;
            auto density = settings.density;
            const auto& scale = settings.scale;

            // 0.05f is the default convex radius of Jolt
            auto convexRadius = glm::min(glm::min(halfHeight, radius) - epsilon, 0.05f);

            JPH::CylinderShapeSettings cylinderShapeSettings(halfHeight, radius, convexRadius);
            cylinderShapeSettings.SetDensity(density);

            auto result = cylinderShapeSettings.Create();

            if (!result.IsValid())
                return false;

            shape->ref = result.Get();

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return shape->Scale(scale);
            }

            return true;

        }

        bool ShapesManager::TryCreateShapeFromCompound(Shape* shape, const CompoundShapeSettings& settings) {

            const auto& scale = settings.scale;

            JPH::StaticCompoundShapeSettings compoundShapeSettings;
            for (const auto& child : settings.children) {
                if (!child.shape)
                    continue;

                // Children are created again, their settings might have changed
                if (!child.shape->TryCreate())
                    return false;

                compoundShapeSettings.AddShape(VecToJPHVec(child.translation),
                    QuatToJPHQuat(child.rotation), child.shape->ref);
            }

            if (compoundShapeSettings.mSubShapes.empty())
                return false;

            auto result = compoundShapeSettings.Create();

            if (!result.IsValid())
                return false;

            shape->ref = result.Get();

            if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f) {
                return shape->Scale(scale);
            }

            return true;

        }

        void ShapesManager::Update() {

            {
                std::scoped_lock lock(meshShapeCacheMutex);
                std::erase_if(meshShapeCache, [](auto& item) { return item.second.resource.expired(); } );
            }

            {
                std::scoped_lock lock(convexHullShapeCacheMutex);
                std::erase_if(convexHullShapeCache, [](auto& item) { return item.second.resource.expired(); } );
            }

            // Height fields aren't owned by a resource, they are unused when only the cache references them
            std::scoped_lock lock(heightFieldShapeCacheMutex);
            std::erase_if(heightFieldShapeCache, [](auto& item) { return item.second->GetRefCount() == 1; });

        }

    }

}
