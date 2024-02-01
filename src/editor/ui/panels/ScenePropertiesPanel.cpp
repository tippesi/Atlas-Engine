#include "ScenePropertiesPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    void ScenePropertiesPanel::Render(Scene::Entity entity) {

        ImGui::Begin(GetNameID());

        isFocused = ImGui::IsWindowFocused();

        if (entity.IsValid()) {



        }

        ImGui::End();

    }

}