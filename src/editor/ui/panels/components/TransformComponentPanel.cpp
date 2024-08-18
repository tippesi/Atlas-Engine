#include "TransformComponentPanel.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace Atlas::Editor::UI {

    bool TransformComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, TransformComponent &transform) {

        ImGui::PushID(GetNameID());

        // Make sure the local matrix is updated from the global one
        // This needs to happen when external system like physics just updated the globalMatrix
        auto parentEntity = scene->GetParentEntity(entity);
        transform.ReconstructLocalMatrix(parentEntity);

        // The matrix decomposition/composition code is a bit unstable and
        // we work with fixed information that is recomposed when changed,
        // but only decomposed when the entity changes. Note, that all
        // component panels are unique per scene window
        if (lastEntity != entity || ImGuizmo::IsUsing()) {
            auto decomposition = transform.Decompose();
            position = decomposition.translation;
            rotation = decomposition.rotation;
            scale = decomposition.scale;
            lastEntity = entity;
        }

        vec3 localPosition = position, localRotation = rotation, localScale = scale;

        ImGui::DragFloat3("Position", &position[0], 0.01f);
        ImGui::DragFloat3("Rotation", &rotation[0], 0.01f);
        ImGui::DragFloat3("Scale", &scale[0], 0.01f, -100.0f, 100.0f);

        // Only recompose when a local change happened
        if (localPosition != position || localScale != scale ||
            localRotation != rotation) {
            Common::MatrixDecomposition composition;
            composition.translation = position;
            composition.rotation = rotation;
            composition.scale = scale;
            transform.Set(composition.Compose());
            lastTransform = transform.matrix;
        }

        ImGui::PopID();

        return false;

    }

}