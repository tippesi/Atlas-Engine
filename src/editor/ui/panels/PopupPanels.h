#pragma once

namespace Atlas::Editor::UI {

    class PopupPanels {

    public:
        static void Render();

        static bool isNewScenePopupVisible;

    private:
        static void RenderNewScenePopup();

        static void SetupPopupSize(float horizontalFactor, float verticalFactor);

    };

}