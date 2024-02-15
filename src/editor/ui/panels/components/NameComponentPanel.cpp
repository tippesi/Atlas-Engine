#include "NameComponentPanel.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace Atlas::Editor::UI {

    bool NameComponentPanel::Render(Ref<Scene::Scene>& scene,
        Scene::Entity entity, NameComponent &nameComponent) {

        ImGui::InputText("Name", &nameComponent.name);

        return false;

    }

}