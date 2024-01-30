#include "SceneHierarchyPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    void SceneHierarchyPanel::Render(Ref<Scene::Scene> &scene) {

        ImGui::Begin(GetNameID());

        if (scene != nullptr) {



        }

        ImGui::End();

    }

}