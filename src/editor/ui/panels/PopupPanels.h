#pragma once

#include <string>
#include <imgui.h>
#include <glm/glm.hpp>

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

        // Bounds and depth of the octree of a new scene with the size of its cells
        static void RenderOctreeSettings(vec3& min, vec3& max, int32_t& depth);

        static void RenderNewScenePopup();

        static void RenderImportScenePopup();

        static void RenderBlockingPopup();

    };

}