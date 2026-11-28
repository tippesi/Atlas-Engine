#include "Guizmo.h"

#include "common/MatrixDecomposition.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace Atlas::Editor {

    void Guizmo::ApplyOffsetToTransform() {

        auto decomp = Common::MatrixDecomposition(transform);
        decomp.translation += offset;
        transform = decomp.Compose();

    }

    void Guizmo::RemoveOffsetFromTransform() {

        auto decomp = Common::MatrixDecomposition(transform);
        decomp.translation -= offset;
        transform = decomp.Compose();

    }

	const float* Guizmo::GetSnappingPtr() {

        float* snappingPtr = nullptr;
        // Expects a 3-comp vector for translation
        if (mode == ImGuizmo::OPERATION::TRANSLATE && snappingEnabled)
            snappingPtr = glm::value_ptr(translationSnap);
        else if (mode == ImGuizmo::OPERATION::ROTATE && snappingEnabled)
            snappingPtr = &rotationSnap;
        else if (mode == ImGuizmo::OPERATION::SCALE && snappingEnabled)
            snappingPtr = &scaleSnap;

        return snappingPtr;

	}

}