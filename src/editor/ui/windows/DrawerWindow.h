#pragma once

#include "Window.h"

namespace Atlas::Editor::UI {

    // Slides up from the status bar and closes when the user continues elsewhere, pinned it stays docked
    class DrawerWindow : public Window {

    public:
        DrawerWindow(const std::string& name, bool show) : Window(name, show) {}

        bool Begin(ImGuiWindowFlags flags = 0);

        void SetPinned(bool pinned);

        // Toggles between the drawer and the window docked in the layout, a docked window shows it in its toolbar
        bool RenderPinButton();

        float GetPinButtonWidth() const;

        // Replaces an open drawer without playing the animations of both
        void TakeOver(DrawerWindow& drawer);

        bool IsVisible() const { return openAmount > 0.0f; }

        bool IsOpening() const { return show && !pinned && openAmount < 1.0f; }

        bool pinned = false;
        float height = 420.0f;

    private:
        void RenderHeader();

        void CloseIfLeft();

        float openAmount = 0.0f;
        bool wasShown = false;
        // Applied before the window begins, it docks or undocks the window
        bool pinToggleRequested = false;

        const float minHeight = 160.0f;
        const float margin = 6.0f;
        const float animationTime = 0.14f;

    };

}
