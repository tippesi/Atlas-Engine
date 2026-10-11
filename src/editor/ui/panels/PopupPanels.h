#pragma once

#include <string>
#include <imgui.h>

namespace Atlas::Editor::UI {

    class PopupPanels {

    public:
        static void Render();

        static void SetupPopupSize(float horizontalFactor, float verticalFactor);

        static bool isNewScenePopupVisible;
        static bool isImportScenePopupVisible;
        static bool isAddComponentPopupVisible;

        static std::string filename;

    private:
        enum class DialogResult {
            None = 0,
            Cancel,
            Confirm
        };

        static constexpr ImGuiWindowFlags dialogFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

        static void SetupDialog();

        // Returns an error message or an empty string if the name is valid
        static std::string ValidateSceneName(const std::string& name);

        static DialogResult RenderDialogFooter(const char* confirmLabel, const std::string& error);

        static void RenderNewScenePopup();

        static void RenderImportScenePopup();

        static void RenderBlockingPopup();

    };

}