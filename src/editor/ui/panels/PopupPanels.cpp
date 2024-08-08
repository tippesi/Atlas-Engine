#include "PopupPanels.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "DataCreator.h"
#include "Singletons.h"
#include "Notifications.h"

namespace Atlas::Editor::UI {

    bool PopupPanels::isNewScenePopupVisible = false;
    bool PopupPanels::isImportScenePopupVisible = false;

    std::string PopupPanels::filename = "";

    void PopupPanels::Render() {

        RenderNewScenePopup();
        RenderImportScenePopup();
        RenderBlockingPopup();

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
        static bool invertUVs = true;
        static bool addRigidBodies = true;
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
                Singletons::blockingOperation->Block("Importing scene. Please wait...",
                    [&]() {
                        auto scene = DataCreator::CreateSceneFromMesh(filename, minSize, maxSize,
                            octreeDepth, invertUVs, addRigidBodies, combineMeshes, makeMeshesStatic);
                        scene->name = name;

                        bool alreadyExisted;
                        Atlas::ResourceManager<Scene::Scene>::AddResource(name, scene, alreadyExisted);

                        if (alreadyExisted) {
                            Log::Warning("Scene couldn't be created due to scene with same name existing already");
                            Notifications::Push({ .message = "Error importing scene " + scene->name });
                            return;
                        }

                        Notifications::Push({ .message = "Imported scene " + scene->name });                        
                    });

                isImportScenePopupVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();

        }

    }

    void PopupPanels::RenderBlockingPopup() {

        if (!ImGui::IsPopupOpen("Blocking popup") && !Singletons::blockingOperation->block)
            return;

        if (!ImGui::IsPopupOpen("Blocking popup")) {
            ImGui::OpenPopup("Blocking popup");
        }

        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 2.0f);

        SetupPopupSize(0.2f, 0.05f);

        if (ImGui::BeginPopupModal("Blocking popup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {

            ImGui::SetKeyboardFocusHere();

            ImGui::Text(Singletons::blockingOperation->blockText.c_str());

            if (!Singletons::blockingOperation->block)
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();

        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();

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