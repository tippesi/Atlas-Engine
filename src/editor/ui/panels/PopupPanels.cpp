#include "PopupPanels.h"

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>

#include "DataCreator.h"
#include "Singletons.h"
#include "Notifications.h"
#include "Theme.h"
#include "loader/AssetLoader.h"

#include <filesystem>
#include <algorithm>

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

        bool popupNew = false;

        if (!ImGui::IsPopupOpen("New scene")) {
            popupNew = true;
            ImGui::OpenPopup("New scene");
        }

        SetupDialog();

        if (ImGui::BeginPopupModal("New scene", nullptr, dialogFlags)) {

            ImGui::SeparatorText("Scene");

            if (popupNew)
                ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("Name", "Type scene name here", &name);

            ImGui::SeparatorText("Bounds");
            ImGui::DragFloat3("Min size", &minSize[0], 1.0f, -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::DragFloat3("Max size", &maxSize[0], 1.0f, -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderInt("Octree depth", &octreeDepth, 1, 12);

            auto confirm = RenderDialogFooter("Create", ValidateSceneName(name));

            if (confirm == DialogResult::Confirm) {
                auto scene = DataCreator::CreateScene(name, minSize, maxSize, octreeDepth);

                bool alreadyExisted;
                Atlas::ResourceManager<Scene::Scene>::AddResource("scenes/" + name + ".aescene", scene, alreadyExisted);

                if (alreadyExisted) {
                    Log::Warning("Scene couldn't be created due to scene with same name existing already");
                }
            }

            if (confirm != DialogResult::None) {
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
        static bool makeMeshesStatic = false;
        static bool convertTexturesToKtx = false;

        bool popupNew = false;

        if (!ImGui::IsPopupOpen("Import scene")) {
            popupNew = true;
            ImGui::OpenPopup("Import scene");
        }

        SetupDialog();

        if (ImGui::BeginPopupModal("Import scene", nullptr, dialogFlags)) {

            if (popupNew)
                name = Common::Path::GetFileNameWithoutExtension(filename);

            ImGui::TextDisabled("Source: %s", filename.c_str());

            ImGui::SeparatorText("Scene");
            if (popupNew)
                ImGui::SetKeyboardFocusHere();
            ImGui::InputTextWithHint("Name", "Type scene name here", &name);

            ImGui::SeparatorText("Bounds");
            ImGui::DragFloat3("Min size", &minSize[0], 1.0f, -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::DragFloat3("Max size", &maxSize[0], 1.0f, -8192.0f, 8192.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderInt("Octree depth", &octreeDepth, 1, 12);

            ImGui::SeparatorText("Import");
            ImGui::Checkbox("Invert UVs", &invertUVs);
            ImGui::Checkbox("Attach mesh rigid body components", &addRigidBodies);
            ImGui::Checkbox("Make meshes static", &makeMeshesStatic);
            ImGui::Checkbox("Convert textures to KTX2", &convertTexturesToKtx);

            auto confirm = RenderDialogFooter("Import", ValidateSceneName(name));

            if (confirm == DialogResult::Confirm) {
                Singletons::blockingOperation->Block("Importing scene. Please wait...",
                    [&]() {
                        auto textureFormat = convertTexturesToKtx ? Loader::ImportTextureFormat::KTX2 :
                            Loader::ImportTextureFormat::Original;
                        auto scene = DataCreator::CreateSceneFromMesh(filename, minSize, maxSize,
                            octreeDepth, invertUVs, addRigidBodies, makeMeshesStatic, textureFormat);
                        scene->name = name;

                        bool alreadyExisted;
                        Atlas::ResourceManager<Scene::Scene>::AddResource("scenes/" + name + ".aescene", scene, alreadyExisted);

                        if (alreadyExisted) {
                            Log::Warning("Scene couldn't be created due to scene with same name existing already");
                            Notifications::Push({ .message = "Error importing scene " + scene->name });
                            return;
                        }

                        Notifications::Push({ .message = "Imported scene " + scene->name });
                    });
            }

            if (confirm != DialogResult::None) {
                isImportScenePopupVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();

        }

    }

    void PopupPanels::SetupDialog() {

        // Centered, fixed width and height fitting the content
        auto viewport = ImGui::GetMainViewport();
        auto center = viewport->GetCenter();
        auto width = std::clamp(viewport->Size.x * 0.35f, 420.0f, 720.0f);

        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.0f), ImVec2(width, FLT_MAX));

    }

    std::string PopupPanels::ValidateSceneName(const std::string& name) {

        if (name.empty())
            return "Enter a name for the scene";

        if (name.find_first_of("/\\:*?\"<>|") != std::string::npos)
            return "The name contains invalid characters";

        auto path = Loader::AssetLoader::GetFullPath("scenes/" + name + ".aescene");
        if (std::filesystem::exists(path))
            return "A scene with this name already exists";

        return "";

    }

    PopupPanels::DialogResult PopupPanels::RenderDialogFooter(const char* confirmLabel, const std::string& error) {

        auto& style = ImGui::GetStyle();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool canConfirm = error.empty();
        if (!canConfirm)
            ImGui::TextColored(Theme::Warning(), "%s", error.c_str());
        else
            ImGui::TextUnformatted("");

        // Buttons are right aligned, the primary action is the rightmost one
        auto buttonWidth = std::max(ImGui::CalcTextSize(confirmLabel).x, ImGui::CalcTextSize("Cancel").x) +
            4.0f * style.FramePadding.x;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - 2.0f * buttonWidth - style.ItemSpacing.x);

        auto result = DialogResult::None;

        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            result = DialogResult::Cancel;

        ImGui::SameLine();

        ImGui::BeginDisabled(!canConfirm);
        Theme::PushPrimaryButton();
        if (ImGui::Button(confirmLabel, ImVec2(buttonWidth, 0.0f)) ||
            canConfirm && ImGui::IsKeyReleased(ImGuiKey_Enter))
            result = DialogResult::Confirm;
        Theme::PopPrimaryButton();
        ImGui::EndDisabled();

        if (!canConfirm && result == DialogResult::Confirm)
            result = DialogResult::None;

        return result;

    }

    void PopupPanels::RenderBlockingPopup() {

        if (!ImGui::IsPopupOpen("Blocking popup") && !Singletons::blockingOperation->block)
            return;

        if (!ImGui::IsPopupOpen("Blocking popup")) {
            ImGui::OpenPopup("Blocking popup");
        }

        ImGui::PushStyleColor(ImGuiCol_Border, Theme::Accent(0.6f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);

        SetupPopupSize(0.2f, 0.05f);

        if (ImGui::BeginPopupModal("Blocking popup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {

            ImGui::SetKeyboardFocusHere();

            // Center the text in the popup
            auto& blockText = Singletons::blockingOperation->blockText;
            auto textSize = ImGui::CalcTextSize(blockText.c_str());
            auto windowSize = ImGui::GetWindowSize();
            ImGui::SetCursorPos(ImVec2((windowSize.x - textSize.x) * 0.5f, (windowSize.y - textSize.y) * 0.5f));
            ImGui::TextUnformatted(blockText.c_str());

            if (!Singletons::blockingOperation->block)
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();

        }

        ImGui::PopStyleVar();
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