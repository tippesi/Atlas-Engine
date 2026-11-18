#pragma once

#include "../System.h"
#include "Shape.h"

#include "../mesh/Mesh.h"
#include "../volume/AABB.h"

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>

#include <Jolt/Physics/SoftBody/SoftBodySharedSettings.h>

namespace Atlas::Physics {

    struct SoftBodySettings;
    struct SoftBodyGeometry;

    class ShapesManager {

    public:
        template<class T>
        static Ref<Shape> CreateShape(const T& shapeSettings);

        template<class T>
        static T GetShapeSettings(ShapeRef shapeRef);

        static void Update();

        // Builds the shape of the mesh in a job, returns true once shapes of the mesh can be created without building it
        static bool PrepareMeshShape(const ResourceHandle<Mesh::Mesh>& mesh);

        // Like a shape for rigid bodies, soft bodies with the same geometry and constraint settings share them. Thread
        // safe, the same settings are only built once at a time
        static JPH::Ref<JPH::SoftBodySharedSettings> GetSoftBodySharedSettings(const SoftBodySettings& settings,
            const SoftBodyGeometry& geometry);

    private:
        template<class T>
        struct CacheItem {
            Weak<T> resource;
            ShapeRef ref;
        };

        struct SoftBodySettingsItem {
            // Distinguishes geometries with the same hash
            size_t vertexCount = 0;
            size_t indexCount = 0;
            std::once_flag built;
            std::atomic_bool isBuilt = false;
            JPH::Ref<JPH::SoftBodySharedSettings> settings;
        };

        static JPH::Ref<JPH::SoftBodySharedSettings> CreateSoftBodySharedSettings(const SoftBodySettings& settings,
            const SoftBodyGeometry& geometry);

        static bool TryCreateShapeFromMesh(Shape* shape, const MeshShapeSettings& settings);
        static bool TryCreateShapeFromAABB(Shape* shape, const BoundingBoxShapeSettings& settings);
        static bool TryCreateShapeFromSphere(Shape* shape, const SphereShapeSettings& settings);
        static bool TryCreateShapeFromCapsule(Shape* shape, const CapsuleShapeSettings& settings);
        static bool TryCreateShapeFromHeightField(Shape* shape, const HeightFieldShapeSettings& settings);
        static bool TryCreateShapeFromConvexHull(Shape* shape, const ConvexHullShapeSettings& settings);
        static bool TryCreateShapeFromCylinder(Shape* shape, const CylinderShapeSettings& settings);
        static bool TryCreateShapeFromCompound(Shape* shape, const CompoundShapeSettings& settings);

        static std::unordered_map<Hash, CacheItem<Mesh::Mesh>> meshShapeCache;
        static std::mutex meshShapeCacheMutex;
        static std::unordered_set<Hash> pendingMeshShapes;
        static JobGroup meshShapeJobs;

        static std::unordered_map<Hash, ShapeRef> heightFieldShapeCache;
        static std::mutex heightFieldShapeCacheMutex;

        static std::unordered_map<Hash, CacheItem<Mesh::Mesh>> convexHullShapeCache;
        static std::mutex convexHullShapeCacheMutex;

        static std::unordered_map<Hash, Ref<SoftBodySettingsItem>> softBodySettingsCache;
        static std::mutex softBodySettingsCacheMutex;

        static const float epsilon;

        friend Shape;

    };

    template<class T>
    Ref<Shape> ShapesManager::CreateShape(const T& shapeSettings) {

        bool success;
        auto shape = CreateRef<Shape>();

        if constexpr (std::is_same_v<MeshShapeSettings, T>) {
            shape->type = ShapeType::Mesh;
            success = TryCreateShapeFromMesh(shape.get(), shapeSettings);
            shape->settings = CreateRef<MeshShapeSettings>(shapeSettings);
        }
        else if constexpr (std::is_same_v<BoundingBoxShapeSettings, T>) {
            shape->type = ShapeType::BoundingBox;
            success = TryCreateShapeFromAABB(shape.get(), shapeSettings);
            shape->settings = CreateRef<BoundingBoxShapeSettings>(shapeSettings);
        }
        else if constexpr (std::is_same_v<SphereShapeSettings, T>) {
            shape->type = ShapeType::Sphere;
            success = TryCreateShapeFromSphere(shape.get(), shapeSettings);
            shape->settings = CreateRef<SphereShapeSettings>(shapeSettings);
        }
        else if constexpr (std::is_same_v<CapsuleShapeSettings, T>) {
            shape->type = ShapeType::Capsule;
            success = TryCreateShapeFromCapsule(shape.get(), shapeSettings);
            shape->settings = CreateRef<CapsuleShapeSettings>(shapeSettings);
        }
        else if constexpr (std::is_same_v<HeightFieldShapeSettings, T>) {
            shape->type = ShapeType::HeightField;
            success = TryCreateShapeFromHeightField(shape.get(), shapeSettings);
            shape->settings = CreateRef<HeightFieldShapeSettings>(shapeSettings);
        }
        else if constexpr (std::is_same_v<ConvexHullShapeSettings, T>) {
            shape->type = ShapeType::ConvexHull;
            success = TryCreateShapeFromConvexHull(shape.get(), shapeSettings);
            shape->settings = CreateRef<ConvexHullShapeSettings>(shapeSettings);
        }
        else if constexpr (std::is_same_v<CylinderShapeSettings, T>) {
            shape->type = ShapeType::Cylinder;
            success = TryCreateShapeFromCylinder(shape.get(), shapeSettings);
            shape->settings = CreateRef<CylinderShapeSettings>(shapeSettings);
        }
        else if constexpr (std::is_same_v<CompoundShapeSettings, T>) {
            shape->type = ShapeType::Compound;
            success = TryCreateShapeFromCompound(shape.get(), shapeSettings);
            shape->settings = CreateRef<CompoundShapeSettings>(shapeSettings);
        }

        /*
        if (!success) {
            shape->settings = std::make_unique<ShapeSettings>(shapeSettings);
        }
        */

        return shape;

    }

    template<class T>
    T ShapesManager::GetShapeSettings(ShapeRef shapeRef) {

        if constexpr (std::is_same_v<MeshShapeSettings, T>) {

        }
        else if constexpr (std::is_same_v<BoundingBoxShapeSettings, T>) {

        }
        else if constexpr (std::is_same_v<SphereShapeSettings, T>) {

        }
        else if constexpr (std::is_same_v<HeightFieldShapeSettings, T>) {

        }

    }

}