#include "PopupPanels.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    bool PopupPanels::isNewScenePopupVisible = false;

    void PopupPanels::Render() {

        RenderNewScenePopup();

    }

    void PopupPanels::RenderNewScenePopup() {

        if (!isNewScenePopupVisible)
            return;

        static std::string name;
        static auto minSize = glm::vec3(-2048.0f);
        static auto maxSize = glm::vec3(2048.0f);
        static int32_t octreeDepth = 5;

        SetupPopupSize(0.6f, 0.3f);

        if (!ImGui::IsPopupOpen("New scene")) {
            ImGui::OpenPopup("New scene");
        }

        if (ImGui::BeginPopupModal("New scene", nullptr, ImGuiWindowFlags_NoResize)) {

            ImGui::InputTextWithHint("Name", "Type scene name here",  &name);
            ImGui::SliderFloat3("Min size", &minSize[0], -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat3("Max size", &maxSize[0], -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderInt("Octree depth", &octreeDepth, 1, 12);

            if (ImGui::Button("Cancel")) {
                isNewScenePopupVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Ok")) {
                auto scene = CreateRef<Scene::Scene>(name, minSize, maxSize, octreeDepth);

                bool alreadyExisted;
                Atlas::ResourceManager<Scene::Scene>::AddResource(name, scene, alreadyExisted);

                if (alreadyExisted) {
                    Log::Warning("Scene couldn't be created due to scene with same name existing already");
                }

                isNewScenePopupVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();

        }

    }

    void PopupPanels::SetupPopupSize(float horizontalFactor, float verticalFactor) {

        auto viewport = ImGui::GetMainViewport();

        auto pos = ImVec2(viewport->Size.x - horizontalFactor * viewport->Size.x,
            viewport->Size.y - verticalFactor * viewport->Size.y);
        pos.x *= 0.5f;
        pos.y *= 0.5f;

        auto size = ImVec2(horizontalFactor * viewport->Size.x,
            verticalFactor * viewport->Size.y);

        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(size);

    }

}