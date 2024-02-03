#include "TransformComponentPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    bool TransformComponentPanel::Render(TransformComponent &transform) {

        auto decomposition = transform.Decompose();

        ImGui::DragFloat3("Position", &decomposition.translation[0], 0.01f);
        ImGui::DragFloat3("Rotation", &decomposition.rotation[0], 0.01f);
        ImGui::DragFloat3("Scale", &decomposition.scale[0], 0.01f, 0.01f, 100.0f);

        transform.Set(decomposition.Compose());

        return false;

    }

}