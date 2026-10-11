#include "HierarchyComponentPanel.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace Atlas::Editor::UI {

    bool HierarchyComponentPanel::Render(const Ref<Scene::Scene>& scene,
        Scene::Entity entity, HierarchyComponent& component) {

        auto childCount = int32_t(component.GetChildren().size());
        ImGui::TextDisabled("%d %s", childCount, childCount == 1 ? "child" : "children");

        return false;

    }

}
