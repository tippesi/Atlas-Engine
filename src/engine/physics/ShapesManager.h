#pragma once

#include "../System.h"
#include "Shape.h"

#include "../mesh/Mesh.h"
#include "../volume/AABB.h"

#include <unordered_map>
#include <mutex>

namespace Atlas::Physics {

    class ShapesManager {

    public:
        template<class T>
        static Ref<Shape> CreateShape(const T& shapeSettings);

        template<class T>
        static T GetShapeSettings(ShapeRef shapeRef);

    private:
        static bool TryCreateShapeFromMesh(Shape* shape, const MeshShapeSettings& settings);
        static bool TryCreateShapeFromAABB(Shape* shape, const BoundingBoxShapeSettings& settings);
        static bool TryCreateShapeFromSphere(Shape* shape, const SphereShapeSettings& settings);
        static bool TryCreateShapeFromCapsule(Shape* shape, const CapsuleShapeSettings& settings);
        static bool TryCreateShapeFromHeightField(Shape* shape, const HeightFieldShapeSettings& settings);

        static bool CreateShapeScaled(Shape* shape, ShapeRef shapeRef, vec3 scale);

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