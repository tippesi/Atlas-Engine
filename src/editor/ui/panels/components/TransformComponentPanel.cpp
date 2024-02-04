#include "TransformComponentPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool TransformComponentPanel::Render(Scene::Entity entity, TransformComponent &transform) {

        // The matrix decomposition/composition code is a bit unstable and
        // we work with fixed information that is recomposed when changed,
        // but only decomposed when the entity changes. Note, that all
        // component panels are unique per scene window
        if (lastEntity != entity) {
            auto decomposition = transform.Decompose();
            position = decomposition.translation;
            rotation = decomposition.rotation;
            scale = decomposition.scale;
        }

        vec3 localPosition = position, localRotation = rotation, localScale = scale;

        ImGui::DragFloat3("Position", &position[0], 0.01f);
        ImGui::DragFloat3("Rotation", &rotation[0], 0.01f);
        ImGui::DragFloat3("Scale", &scale[0], 0.01f, 0.01f, 100.0f);

        // Only recompose when a local change happened
        if (localPosition != position || localScale != scale ||
            localRotation != rotation) {
            Common::MatrixDecomposition composition;
            composition.translation = position;
            composition.rotation = rotation;
            composition.scale = scale;
            transform.Set(composition.Compose());
        }

        return false;

    }

}