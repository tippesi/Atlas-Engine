#pragma once

#include <string>

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
        static void RenderNewScenePopup();

        static void RenderImportScenePopup();

        static void RenderBlockingPopup();

    };

}