#include "EntityPropertiesPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    void EntityPropertiesPanel::Render(Scene::Entity entity) {

        ImGui::Begin(GetNameID());

        if (entity.IsValid()) {



        }

        ImGui::End();

    }

}