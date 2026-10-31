#include "Shape.h"
#include "ShapesManager.h"
#include "MathConversion.h"

#include <Jolt/Physics/Collision/Shape/ScaledShape.h>

namespace Atlas::Physics {

    bool Shape::TryCreate() {

        if (type == ShapeType::Mesh) {
            auto meshSettings = static_cast<MeshShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromMesh(this, *meshSettings);
        }
        else if (type == ShapeType::Sphere) {
            auto sphereSettings = static_cast<SphereShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromSphere(this, *sphereSettings);
        }
        else if (type == ShapeType::BoundingBox) {
            auto boundingBoxSettings = static_cast<BoundingBoxShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromAABB(this, *boundingBoxSettings);
        }
        else if (type == ShapeType::Capsule) {
            auto capsuleSettings = static_cast<CapsuleShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromCapsule(this, *capsuleSettings);
        }
        else if (type == ShapeType::HeightField) {
            auto heightFieldSettings = static_cast<HeightFieldShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromHeightField(this, *heightFieldSettings);
        }
        else if (type == ShapeType::ConvexHull) {
            auto convexHullSettings = static_cast<ConvexHullShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromConvexHull(this, *convexHullSettings);
        }
        else if (type == ShapeType::Cylinder) {
            auto cylinderSettings = static_cast<CylinderShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromCylinder(this, *cylinderSettings);
        }
        else if (type == ShapeType::Compound) {
            auto compoundSettings = static_cast<CompoundShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromCompound(this, *compoundSettings);
        }

        return false;

    }

    bool Shape::Scale(const vec3& scale) {
        
        if (!IsValid())
            return false;

        if (ref->GetSubType() == JPH::EShapeSubType::Scaled) {
            // Use temp ref to own the memory
            auto tmpRef = ref;
            auto shape = static_cast<const JPH::ScaledShape*>(tmpRef.GetPtr());
            // Kind of a scaled shape "reset"
            ref = shape->GetInnerShape();
        }

        // E.g. spheres need a uniform scale, cylinders the same scale on x and z
        auto correctedScale = JPHVecToVec(ref->MakeScaleValid(VecToJPHVec(scale)));

        JPH::ScaledShapeSettings scaledShapeSettings(ref, VecToJPHVec(correctedScale));
        auto result = scaledShapeSettings.Create();

        if (!result.IsValid())
            return false;

        ref = result.Get();

        if (settings) {
            if (type == ShapeType::Mesh) {
                static_cast<MeshShapeSettings*>(settings.get())->scale = correctedScale;
            }
            else if (type == ShapeType::Sphere) {
                static_cast<SphereShapeSettings*>(settings.get())->scale = correctedScale;
            }
            else if (type == ShapeType::BoundingBox) {
                static_cast<BoundingBoxShapeSettings*>(settings.get())->scale = correctedScale;
            }
            else if (type == ShapeType::Capsule) {
                static_cast<CapsuleShapeSettings*>(settings.get())->scale = correctedScale;
            }
            else if (type == ShapeType::HeightField) {
                static_cast<HeightFieldShapeSettings*>(settings.get())->scale = correctedScale;
            }
            else if (type == ShapeType::ConvexHull) {
                static_cast<ConvexHullShapeSettings*>(settings.get())->scale = correctedScale;
            }
            else if (type == ShapeType::Cylinder) {
                static_cast<CylinderShapeSettings*>(settings.get())->scale = correctedScale;
            }
            else if (type == ShapeType::Compound) {
                static_cast<CompoundShapeSettings*>(settings.get())->scale = correctedScale;
            }
        }

        return true;

    }

}
