#include "PopupPanels.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "DataCreator.h"

namespace Atlas::Editor::UI {

    bool PopupPanels::isNewScenePopupVisible = false;
    bool PopupPanels::isImportScenePopupVisible = false;

    std::string PopupPanels::filename = "";

    void PopupPanels::Render() {

        RenderNewScenePopup();
        RenderImportScenePopup();

    }

    void PopupPanels::RenderNewScenePopup() {

        if (!isNewScenePopupVisible)
            return;

        static std::string name;
        static auto minSize = glm::vec3(-2048.0f);
        static auto maxSize = glm::vec3(2048.0f);
        static int32_t octreeDepth = 5;

        SetupPopupSize(0.6f, 0.3f);

        bool popupNew = false;

        if (!ImGui::IsPopupOpen("New scene")) {
            popupNew = true;
            ImGui::OpenPopup("New scene");
        }

        if (ImGui::BeginPopupModal("New scene", nullptr, ImGuiWindowFlags_NoResize)) {

            if (popupNew)
                ImGui::SetKeyboardFocusHere();
                
            ImGui::InputTextWithHint("Name", "Type scene name here",  &name);

            ImGui::DragFloat3("Min size", &minSize[0], 1.0f, -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::DragFloat3("Max size", &maxSize[0], 1.0f, -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderInt("Octree depth", &octreeDepth, 1, 12);

            if (ImGui::Button("Cancel")) {
                isNewScenePopupVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Ok") || ImGui::IsKeyReleased(ImGuiKey_Enter)) {
                auto scene = DataCreator::CreateScene(name, minSize, maxSize, octreeDepth);

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

    void PopupPanels::RenderImportScenePopup() {

        if (!isImportScenePopupVisible)
            return;

        static std::string name;
        static auto minSize = glm::vec3(-2048.0f);
        static auto maxSize = glm::vec3(2048.0f);
        static int32_t octreeDepth = 5;
        static bool invertUVs = false;
        static bool addRigidBodies = false;
        static bool combineMeshes = false;
        static bool makeMeshesStatic = false;

        SetupPopupSize(0.6f, 0.3f);

        bool popupNew = false;

        if (!ImGui::IsPopupOpen("Import scene")) {
            popupNew = true;
            ImGui::OpenPopup("Import scene");
        }

        if (ImGui::BeginPopupModal("Import scene", nullptr, ImGuiWindowFlags_NoResize)) {

            if (popupNew) {
                ImGui::SetKeyboardFocusHere();
                name = Common::Path::GetFileNameWithoutExtension(filename);
            }

            ImGui::Text("General properties");
                
            ImGui::InputTextWithHint("Name", "Type scene name here",  &name);

            ImGui::DragFloat3("Min size", &minSize[0], 1.0f, -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::DragFloat3("Max size", &maxSize[0], 1.0f, -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderInt("Octree depth", &octreeDepth, 1, 12);

            ImGui::Text("Import properties");
            ImGui::Checkbox("Invert UVs", &invertUVs);
            ImGui::Checkbox("Attach mesh rigid body components", &addRigidBodies);
            ImGui::Checkbox("Combine meshes", &combineMeshes);
            ImGui::Checkbox("Make meshes static", &makeMeshesStatic);

            if (ImGui::Button("Cancel")) {
                isImportScenePopupVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Ok") || ImGui::IsKeyReleased(ImGuiKey_Enter)) {
                auto scene = DataCreator::CreateSceneFromMesh(filename, minSize, maxSize, 
                    octreeDepth, invertUVs, addRigidBodies, combineMeshes, makeMeshesStatic);
                scene->name = name;

                bool alreadyExisted;
                Atlas::ResourceManager<Scene::Scene>::AddResource(name, scene, alreadyExisted);

                if (alreadyExisted) {
                    Log::Warning("Scene couldn't be created due to scene with same name existing already");
                }

                isImportScenePopupVisible = false;
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