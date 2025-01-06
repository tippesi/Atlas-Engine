#include "HierarchyComponentPanel.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace Atlas::Editor::UI {

    bool HierarchyComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, HierarchyComponent& component) {

        ImGui::Text("Number of children: %d", int32_t(component.GetChildren().size()));

        return false;

    }

}