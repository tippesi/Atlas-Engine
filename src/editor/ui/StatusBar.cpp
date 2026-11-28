#include "StatusBar.h"
#include "../Workspace.h"
#include "../Singletons.h"
#include "../Notifications.h"
#include "../Theme.h"

#include <ImGuizmo.h>
#include <imgui_internal.h>

#include <algorithm>

namespace Atlas::Editor::UI {

    void StatusBar::Render() {

        auto viewport = ImGui::GetMainViewport();
        auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

        auto gap = [](float width) {
            ImGui::SameLine(0.0f, width);
            };

        // Plain text shares the vertical center with the labels of the chips
        auto text = [&](const char* str, ImGuiCol color) {
            auto pos = ImGui::GetCursorScreenPos();
            auto height = ImGui::GetFrameHeight();
            ImGui::Dummy(ImVec2(ImGui::CalcTextSize(str).x, height));
            ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, GetCapCenteredTextY(pos.y + height * 0.5f)),
                ImGui::GetColorU32(color), str);
            };

        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, Theme::Base());
        if (ImGui::BeginViewportSideBar("##Status bar", viewport, ImGuiDir_Down, ImGui::GetFrameHeight() + 4.0f, flags)) {
            if (ImGui::BeginMenuBar()) {
                auto window = ImGui::GetCurrentWindow();
                window->DrawList->AddLine(window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y),
                    ImGui::GetColorU32(ImGuiCol_Border));

                auto warningCount = workspace.logWindow.GetUnseenWarningCount();
                auto errorCount = workspace.logWindow.GetUnseenErrorCount();

                // The drawers chosen for the status bar, a right click on it changes them
                auto contentBrowserHovered = false;
                auto firstDrawer = true;
                for (const auto& name : Singletons::config->statusBarDrawers) {
                    auto drawer = workspace.FindDrawer(name);
                    if (!drawer)
                        continue;
                    if (!firstDrawer)
                        gap(4.0f);
                    firstDrawer = false;

                    auto isLog = drawer->window == &workspace.logWindow;
                    auto hovered = RenderDrawerButton(*drawer->window, drawer->icon, drawer->name, drawer->command,
                        isLog ? errorCount : 0, isLog ? warningCount : 0);
                    if (drawer->window == &workspace.contentBrowserWindow)
                        contentBrowserHovered = hovered;
                }

                // Dragging something onto the button opens the content browser as a drop target
                auto isDragging = ImGui::IsDragDropActive();
                drawerSpringTime = isDragging && contentBrowserHovered ? drawerSpringTime + ImGui::GetIO().DeltaTime : 0.0f;
                if (drawerSpringTime > 0.35f)
                    workspace.contentBrowserWindow.show = true;

                RenderMenu();

                gap(16.0f);

                // Context is plain information, it stays quiet next to the things that can be clicked
                auto sceneWindow = workspace.GetActiveSceneWindow();
                std::string guizmoText;
                if (sceneWindow && sceneWindow->scene.IsLoaded()) {
                    if (sceneWindow->isPlaying) {
                        RenderChip("##Playing", IconType::Play, "Playing", Theme::Success(), Theme::Success(0.16f), Theme::Success(0.16f));
                        gap(8.0f);
                    }

                    auto sceneLabel = sceneWindow->IsPrefab() ? Common::Path::GetFileName(sceneWindow->prefabPath) :
                        UI::SceneWindow::GetSceneLabel(sceneWindow->scene);
                    text(sceneLabel.c_str(), ImGuiCol_Text);

                    const auto& selection = sceneWindow->sceneHierarchyPanel.selectedEntities;
                    auto selectedEntity = sceneWindow->sceneHierarchyPanel.selectedEntity;
                    if (!selection.empty()) {
                        gap(6.0f);
                        text("/", ImGuiCol_TextDisabled);
                        gap(6.0f);
                        if (selection.size() == 1 && selectedEntity.IsValid()) {
                            auto nameComponent = selectedEntity.TryGetComponent<NameComponent>();
                            auto name = nameComponent ? nameComponent->name : "Entity " + std::to_string(selectedEntity);
                            text(name.c_str(), ImGuiCol_Text);
                        }
                        else {
                            text((std::to_string(selection.size()) + " entities").c_str(), ImGuiCol_Text);
                        }
                    }

                    const auto& guizmo = sceneWindow->guizmo;
                    guizmoText = guizmo.mode == ImGuizmo::OPERATION::ROTATE ? "Rotate" :
                        guizmo.mode == ImGuizmo::OPERATION::SCALE ? "Scale" : "Translate";
                    guizmoText += guizmo.objectSpace ? " \xC2\xB7 Local" : " \xC2\xB7 World";
                    if (guizmo.snappingEnabled)
                        guizmoText += " \xC2\xB7 Snapping";
                }
                else {
                    text("No scene open", ImGuiCol_TextDisabled);
                }

                // Jobs come in bursts, the peak of the last half second avoids a flickering count. Short work isn't mentioned
                auto deltaTime = ImGui::GetIO().DeltaTime;
                auto backgroundJobs = JobSystem::GetPendingJobCount(JobPriority::Medium) +
                    JobSystem::GetPendingJobCount(JobPriority::Low);
                backgroundJobPeak = std::max(backgroundJobPeak, backgroundJobs);

                backgroundJobRefreshTime += deltaTime;
                if (backgroundJobRefreshTime > 0.5f || backgroundJobShown == 0) {
                    backgroundJobShown = backgroundJobPeak;
                    backgroundJobPeak = backgroundJobs;
                    backgroundJobRefreshTime = 0.0f;
                }
                backgroundJobTime = backgroundJobShown > 0 ? backgroundJobTime + deltaTime : 0.0f;

                if (backgroundJobTime > 0.5f && backgroundJobShown > 0) {
                    gap(16.0f);
                    const char* dots[] = { ".", "..", "..." };
                    char jobText[64];
                    std::snprintf(jobText, sizeof(jobText), "%d background %s%s", backgroundJobShown, backgroundJobShown == 1 ? "job" : "jobs",
                        dots[int32_t(ImGui::GetTime() * 2.0) % 3]);
                    text(jobText, ImGuiCol_TextDisabled);
                }

                // Gizmo mode, performance and the notifications on the right side
                auto frameTime = Clock::GetAverage() * 1000.0f;
                char rightText[128];
                std::snprintf(rightText, sizeof(rightText), "%s%s%.1f ms \xC2\xB7 %.0f fps", guizmoText.c_str(),
                    guizmoText.empty() ? "" : "     ", frameTime, frameTime > 0.0f ? 1000.0f / frameTime : 0.0f);
                auto notificationWidth = GetChipWidth("", GetNotificationBadgeWidth(Notifications::GetUnreadCount()));
                auto rightWidth = ImGui::CalcTextSize(rightText).x + ImGui::GetStyle().ItemSpacing.x + notificationWidth;
                auto rightPos = ImGui::GetWindowContentRegionMax().x - rightWidth;

                // The problem nobody has seen yet takes the room in between and opens the log
                if (warningCount + errorCount > 0 && !workspace.logWindow.GetLastProblem().empty()) {
                    gap(16.0f);
                    auto maxWidth = rightPos - ImGui::GetCursorPosX() - 16.0f;
                    auto isError = workspace.logWindow.IsLastProblemError();
                    auto color = isError ? Theme::Error() : Theme::Warning();
                    auto background = isError ? Theme::Error(0.1f) : Theme::Warning(0.1f);
                    auto backgroundHovered = isError ? Theme::Error(0.22f) : Theme::Warning(0.22f);
                    if (maxWidth > 80.0f) {
                        if (RenderChip("##Last problem", isError ? IconType::Error : IconType::Warning,
                            workspace.logWindow.GetLastProblem().c_str(), color, background, backgroundHovered, maxWidth))
                            workspace.logWindow.show = true;
                        ImGui::SetItemTooltip("%s\nClick to open the log", workspace.logWindow.GetLastProblem().c_str());
                    }
                }

                if (rightPos > ImGui::GetCursorPosX()) {
                    ImGui::SameLine(rightPos);
                    text(rightText, ImGuiCol_TextDisabled);
                }

                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - notificationWidth);
                RenderNotificationButton();

                ImGui::EndMenuBar();
            }
        }
        // Side bars always need to be ended
        ImGui::End();
        ImGui::PopStyleColor();

    }

    bool StatusBar::RenderChip(const char* id, IconType icon, const char* text, ImVec4 color,
        ImVec4 background, ImVec4 backgroundHovered, float maxWidth, float trailingWidth) {

        const float padding = 8.0f;
        // Chips without text only show their icon
        const float iconGap = text[0] ? 5.0f : 0.0f;

        auto fontSize = ImGui::GetFontSize();
        auto iconSize = fontSize;
        auto textSize = ImGui::CalcTextSize(text);
        auto width = std::min(GetChipWidth(text, trailingWidth), maxWidth);
        auto height = ImGui::GetFrameHeight();

        // Pressing on click, since a drawer that is open closes itself when the mouse goes down outside of it
        auto pos = ImGui::GetCursorScreenPos();
        auto pressed = ImGui::InvisibleButton(id, ImVec2(width, height), ImGuiButtonFlags_PressedOnClick);
        auto hovered = ImGui::IsItemHovered();

        auto drawList = ImGui::GetWindowDrawList();
        auto min = ImVec2(pos.x, pos.y + 2.0f);
        auto max = ImVec2(pos.x + width, pos.y + height - 2.0f);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(hovered ? backgroundHovered : background), 5.0f);

        auto centerY = (min.y + max.y) * 0.5f;
        auto iconMin = ImVec2(min.x + padding, centerY - iconSize * 0.5f);
        auto set = Singletons::imguiWrapper->GetTextureId(&Singletons::icons->GetTintable(icon));
        drawList->AddImage(set, iconMin, ImVec2(iconMin.x + iconSize, iconMin.y + iconSize),
            ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f), ImGui::GetColorU32(color));

        auto textMin = ImVec2(iconMin.x + iconSize + iconGap, GetCapCenteredTextY(centerY));
        auto textMaxX = max.x - padding - trailingWidth;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::RenderTextEllipsis(drawList, textMin, ImVec2(textMaxX, max.y), textMaxX, textMaxX, text, nullptr, &textSize);
        ImGui::PopStyleColor();

        return pressed;

    }

    float StatusBar::GetChipWidth(const char* text, float trailingWidth) {

        // Padding, icon, the gap to the text and the text, chips without text only show their icon
        auto iconGap = text[0] ? 5.0f : 0.0f;
        return 8.0f + ImGui::GetFontSize() + iconGap + ImGui::CalcTextSize(text).x + trailingWidth + 8.0f;

    }

    float StatusBar::GetNotificationBadgeWidth(int32_t unreadCount) {

        // Gap to the icon and the padded count
        return unreadCount > 0 ? 6.0f + ImGui::CalcTextSize(std::to_string(unreadCount).c_str()).x + 10.0f : 0.0f;

    }

    void StatusBar::RenderNotificationButton() {

        auto unreadCount = Notifications::GetUnreadCount();
        auto hasError = unreadCount > 0 && Notifications::HasUnreadError();
        auto countText = std::to_string(unreadCount);
        auto badgeWidth = GetNotificationBadgeWidth(unreadCount);

        auto active = notificationsOpen || unreadCount > 0;
        auto color = active ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : Theme::TextMuted();
        auto background = notificationsOpen ? Theme::Accent(0.35f) : Theme::TextMuted(0.12f);
        auto backgroundHovered = notificationsOpen ? Theme::Accent(0.45f) : Theme::TextMuted(0.22f);

        // The popup closes itself when the mouse goes down outside of it, a click on the button shouldn't open it again
        auto wasOpen = notificationsOpen;
        if (RenderChip("##Notifications", IconType::Notifications, "", color, background, backgroundHovered,
            FLT_MAX, badgeWidth) && !wasOpen)
            ImGui::OpenPopup("##Notification center");
        if (unreadCount > 0)
            ImGui::SetItemTooltip("%d new %s", unreadCount, unreadCount == 1 ? "notification" : "notifications");
        else
            ImGui::SetItemTooltip("Notifications");

        auto itemMin = ImGui::GetItemRectMin();
        auto itemMax = ImGui::GetItemRectMax();
        if (unreadCount > 0) {
            auto badgeColor = hasError ? Theme::Error() : Theme::Accent();
            auto min = ImVec2(itemMax.x - 8.0f - badgeWidth + 6.0f, itemMin.y + 5.0f);
            auto max = ImVec2(itemMax.x - 8.0f, itemMax.y - 5.0f);
            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(badgeColor), (max.y - min.y) * 0.5f);
            drawList->AddText(ImVec2(min.x + 5.0f, GetCapCenteredTextY((min.y + max.y) * 0.5f)),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), countText.c_str());
        }

        ImGui::SetNextWindowPos(ImVec2(itemMax.x, itemMin.y - 6.0f), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
        notificationsOpen = ImGui::BeginPopup("##Notification center");
        if (notificationsOpen) {
            // Everything that is visible while it is open counts as read
            Notifications::MarkAsRead();
            Notifications::DisplayHistory();
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

    }

    float StatusBar::GetCapCenteredTextY(float centerY) {

        // The line height includes room for descenders, labels look centered when their capitals are
        auto font = ImGui::GetFont();
        auto scale = ImGui::GetFontSize() / font->FontSize;
        auto glyph = font->FindGlyph('H');
        return centerY - (glyph->Y0 + glyph->Y1) * 0.5f * scale;

    }

    bool StatusBar::RenderDrawerButton(UI::DrawerWindow& drawer, IconType icon, const char* label, const char* command,
        int32_t errorCount, int32_t warningCount) {

        const float badgeGap = 6.0f;
        const float badgePadding = 5.0f;
        const float badgeIconGap = 3.0f;

        auto color = drawer.show ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : Theme::TextMuted();
        auto background = drawer.show ? Theme::Accent(0.35f) : Theme::TextMuted(0.12f);
        auto backgroundHovered = drawer.show ? Theme::Accent(0.45f) : Theme::TextMuted(0.22f);

        // New warnings and errors are shown as badges inside of the button
        struct Badge {
            IconType icon;
            ImVec4 color;
            int32_t count;
            const char* name;
            char text[16];
            float width;
        };
        Badge badges[] = {
            { IconType::Error, Theme::Error(), errorCount, "errors" },
            { IconType::Warning, Theme::Warning(), warningCount, "warnings" }
        };

        auto badgeIconSize = ImGui::GetFontSize() * 0.9f;
        auto trailingWidth = 0.0f;
        for (auto& badge : badges) {
            if (badge.count <= 0)
                continue;
            std::snprintf(badge.text, sizeof(badge.text), "%d", badge.count);
            badge.width = badgePadding + badgeIconSize + badgeIconGap + ImGui::CalcTextSize(badge.text).x + badgePadding;
            trailingWidth += badgeGap + badge.width;
        }

        if (RenderChip(label, icon, label, color, background, backgroundHovered, FLT_MAX, trailingWidth))
            drawer.show = !drawer.show;
        auto hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

        auto drawList = ImGui::GetWindowDrawList();
        auto itemMin = ImGui::GetItemRectMin();
        auto itemMax = ImGui::GetItemRectMax();
        auto fontSize = ImGui::GetFontSize();
        auto x = itemMax.x - 8.0f - trailingWidth;
        const Badge* hoveredBadge = nullptr;
        for (const auto& badge : badges) {
            if (badge.count <= 0)
                continue;
            x += badgeGap;
            auto min = ImVec2(x, itemMin.y + 5.0f);
            auto max = ImVec2(x + badge.width, itemMax.y - 5.0f);
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(badge.color.x, badge.color.y, badge.color.z, 0.2f)), 4.0f);

            auto iconMin = ImVec2(min.x + badgePadding, (itemMin.y + itemMax.y - badgeIconSize) * 0.5f);
            auto set = Singletons::imguiWrapper->GetTextureId(&Singletons::icons->GetTintable(badge.icon));
            drawList->AddImage(set, iconMin, ImVec2(iconMin.x + badgeIconSize, iconMin.y + badgeIconSize),
                ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f), ImGui::GetColorU32(badge.color));
            drawList->AddText(ImVec2(iconMin.x + badgeIconSize + badgeIconGap, GetCapCenteredTextY((itemMin.y + itemMax.y) * 0.5f)),
                ImGui::GetColorU32(badge.color), badge.text);

            if (hovered && ImGui::IsMouseHoveringRect(min, max))
                hoveredBadge = &badge;
            x += badge.width;
        }

        if (hoveredBadge)
            ImGui::SetItemTooltip("%d new %s", hoveredBadge->count, hoveredBadge->name);
        else
            ImGui::SetItemTooltip("%s", Singletons::commandPalette->GetTooltip(label, command).c_str());

        return hovered;

    }

    void StatusBar::RenderMenu() {

        if (!ImGui::BeginPopupContextWindow("Status bar menu"))
            return;

        auto& shownDrawers = Singletons::config->statusBarDrawers;

        ImGui::TextDisabled("Drawers in the status bar, drag to reorder");
        ImGui::Separator();

        int32_t removedDrawer = -1, draggedDrawer = -1;
        std::vector<float> rowTops;
        for (int32_t i = 0; i < int32_t(shownDrawers.size()); i++) {
            // The id follows the entry, such that it stays active while it moves past others
            ImGui::PushID(shownDrawers[i].c_str());
            auto shown = true;
            if (ImGui::Checkbox("##Shown", &shown))
                removedDrawer = i;
            ImGui::SameLine();
            ImGui::Selectable(shownDrawers[i].c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups);
            rowTops.push_back(ImGui::GetItemRectMin().y);
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                draggedDrawer = i;
            ImGui::PopID();
        }
        // A dragged entry moves to the row below the mouse
        if (draggedDrawer >= 0 && removedDrawer < 0) {
            int32_t targetDrawer = 0;
            for (int32_t i = 1; i < int32_t(rowTops.size()); i++)
                if (ImGui::GetIO().MousePos.y >= rowTops[i])
                    targetDrawer = i;
            if (targetDrawer != draggedDrawer) {
                auto name = shownDrawers[draggedDrawer];
                shownDrawers.erase(shownDrawers.begin() + draggedDrawer);
                shownDrawers.insert(shownDrawers.begin() + targetDrawer, name);
            }
        }
        if (removedDrawer >= 0)
            shownDrawers.erase(shownDrawers.begin() + removedDrawer);

        // Drawers which aren't shown are added at the end
        for (const auto& drawer : workspace.drawers) {
            if (std::find(shownDrawers.begin(), shownDrawers.end(), drawer.name) != shownDrawers.end())
                continue;
            ImGui::PushID(drawer.name);
            auto shown = false;
            if (ImGui::Checkbox("##Shown", &shown))
                shownDrawers.push_back(drawer.name);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", drawer.name);
            ImGui::PopID();
        }

        ImGui::EndPopup();

    }

}
