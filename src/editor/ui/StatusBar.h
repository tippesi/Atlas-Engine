#pragma once

#include "windows/DrawerWindow.h"
#include "../Icons.h"

#include <imgui.h>

namespace Atlas::Editor {

    class Workspace;

}

namespace Atlas::Editor::UI {

    // The bar at the bottom: drawers, the scene and its selection, background work, performance and notifications
    class StatusBar {

    public:
        explicit StatusBar(Workspace& workspace) : workspace(workspace) {}

        void Render();

    private:
        // Clickable items carry a background, such that they stand out from plain information
        bool RenderChip(const char* id, IconType icon, const char* text, ImVec4 color,
            ImVec4 background, ImVec4 backgroundHovered, float maxWidth = FLT_MAX, float trailingWidth = 0.0f);

        float GetChipWidth(const char* text, float trailingWidth = 0.0f);

        float GetNotificationBadgeWidth(int32_t unreadCount);

        void RenderNotificationButton();

        float GetCapCenteredTextY(float centerY);

        bool RenderDrawerButton(DrawerWindow& drawer, IconType icon, const char* label, const char* command,
            int32_t errorCount = 0, int32_t warningCount = 0);

        // Chooses and orders the drawers, opened by a right click
        void RenderMenu();

        Workspace& workspace;

        float drawerSpringTime = 0.0f;
        // Background job count, updated twice a second
        float backgroundJobTime = 0.0f;
        float backgroundJobRefreshTime = 0.0f;
        int32_t backgroundJobPeak = 0;
        int32_t backgroundJobShown = 0;

        bool notificationsOpen = false;

    };

}
