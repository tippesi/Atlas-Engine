#include "Shape.h"
#include "ShapesManager.h"

namespace Atlas::Physics {

    bool Shape::TryCreate() {

        if (IsValid())
            return true;

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
        else if (type == ShapeType::HeightField) {
            auto heightFieldSettings = static_cast<HeightFieldShapeSettings*>(settings.get());
            return ShapesManager::TryCreateShapeFromHeightField(this, *heightFieldSettings);
        }

        return false;

    }

}