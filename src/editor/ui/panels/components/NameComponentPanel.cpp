#include "NameComponentPanel.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace Atlas::Editor::UI {

    bool NameComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, NameComponent &nameComponent) {

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##Name", "Entity name", &nameComponent.name);

        return false;

    }

}