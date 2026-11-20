#include "HierarchyComponentPanel.h"
#include "../../Widgets.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace Atlas::Editor::UI {

    bool HierarchyComponentPanel::Render(const Ref<Scene::Scene>& scene,
        Scene::Entity entity, HierarchyComponent& component) {

        auto childCount = int32_t(component.GetChildren().size());
        if (Widgets::BeginProperties("HierarchyComponent")) {
            Widgets::PropertyLabel("Children");
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%d", childCount);
            Widgets::EndProperties();
        }

        return false;

    }

}
