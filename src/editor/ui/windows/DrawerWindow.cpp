#include "DrawerWindow.h"

#include "../../Singletons.h"
#include "../../Theme.h"
#include "../../Icons.h"
#include "../Widgets.h"

#include <imgui_internal.h>

#include <algorithm>

namespace Atlas::Editor::UI {

    bool DrawerWindow::Begin(ImGuiWindowFlags flags) {

        if (show && !wasShown)
            ImGui::SetNextWindowFocus();
        wasShown = show;

        if (pinned) {
            openAmount = show ? 1.0f : 0.0f;
            return Window::Begin(flags);
        }

        auto step = ImGui::GetIO().DeltaTime / animationTime;
        openAmount = std::clamp(openAmount + (show ? step : -step), 0.0f, 1.0f);
        if (openAmount <= 0.0f)
            return false;

        auto viewport = ImGui::GetMainViewport();
        auto drawerHeight = std::clamp(height, minHeight, std::max(viewport->WorkSize.y - 2.0f * margin, minHeight));

        // Ease out, the drawer gets slower towards its final height
        auto invOpenAmount = 1.0f - openAmount;
        auto currentHeight = std::max(drawerHeight * (1.0f - invOpenAmount * invOpenAmount * invOpenAmount), 1.0f);

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + margin,
            viewport->WorkPos.y + viewport->WorkSize.y - currentHeight - margin));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - 2.0f * margin, currentHeight));

        flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
        auto visible = ImGui::Begin(GetNameID(), nullptr, flags);
        ImGui::PopStyleVar(2);

        if (!visible) {
            ImGui::End();
            return false;
        }

        RenderHeader();
        CloseIfLeft();

        return true;

    }

    void DrawerWindow::SetPinned(bool pinned) {

        this->pinned = pinned;
        requestDocking = pinned;
        show = pinned || show;
        // An unpinned window opens as a drawer, which closes the other drawer
        if (!pinned)
            openAmount = 0.0f;

    }

    void DrawerWindow::TakeOver(DrawerWindow& drawer) {

        openAmount = std::max(openAmount, drawer.openAmount);
        drawer.openAmount = 0.0f;
        drawer.show = false;
        drawer.wasShown = false;

    }

    void DrawerWindow::RenderHeader() {

        auto window = ImGui::GetCurrentWindow();
        auto drawList = ImGui::GetWindowDrawList();
        auto pos = window->Pos;
        auto size = window->Size;

        // Soft shadow on the layout below to lift the drawer from it
        const float shadowSize = 14.0f;
        auto rounding = ImGui::GetStyle().WindowRounding;
        auto shadowColor = ImGui::GetColorU32(Theme::Shadow());
        drawList->PushClipRectFullScreen();
        drawList->AddRectFilledMultiColor(ImVec2(pos.x + rounding, pos.y - shadowSize),
            ImVec2(pos.x + size.x - rounding, pos.y), 0, 0, shadowColor, shadowColor);
        drawList->PopClipRect();

        // The top edge resizes the drawer
        const float handleHeight = 8.0f;
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton("Drawer resize handle", ImVec2(size.x, handleHeight));
        auto handleActive = ImGui::IsItemActive();
        auto handleHovered = ImGui::IsItemHovered();
        if (handleActive)
            height = size.y - ImGui::GetIO().MouseDelta.y;
        if (handleActive || handleHovered)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

        const float gripWidth = 36.0f;
        auto gripColor = handleActive || handleHovered ? Theme::Accent() : Theme::TextMuted(0.5f);
        auto gripCenter = pos.x + size.x * 0.5f;
        drawList->AddRectFilled(ImVec2(gripCenter - gripWidth * 0.5f, pos.y + 3.0f),
            ImVec2(gripCenter + gripWidth * 0.5f, pos.y + 6.0f), ImGui::GetColorU32(gripColor), 2.0f);

        auto startX = ImGui::GetStyle().WindowPadding.x;
        ImGui::SetCursorPos(ImVec2(startX, handleHeight));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(name.c_str());

        auto& style = ImGui::GetStyle();
        const char* pinLabel = "Dock in layout";
        auto iconSize = ImGui::GetTextLineHeight();
        auto pinWidth = ImGui::CalcTextSize(pinLabel).x + 2.0f * style.FramePadding.x;
        auto closeWidth = iconSize + 2.0f;

        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - pinWidth - closeWidth - style.ItemSpacing.x);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        if (ImGui::Button(pinLabel))
            SetPinned(true);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::SetItemTooltip("Keeps the window open as part of the layout");

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + style.FramePadding.y - 1.0f);
        if (Widgets::IconButton("Close drawer", Singletons::icons->Get(IconType::ArrowDown), iconSize, "Close (Esc)"))
            show = false;

        ImGui::Separator();

    }

    void DrawerWindow::CloseIfLeft() {

        // A drawer which replaced another one is fully open right away, the click that opened it doesn't count
        if (!show || openAmount < 1.0f || ImGui::IsWindowAppearing())
            return;

        auto window = ImGui::GetCurrentWindow();
        auto& g = *ImGui::GetCurrentContext();
        auto& io = ImGui::GetIO();

        // Dragged content gets the layout free as soon as it leaves the drawer, such that it can be dropped there
        if (ImGui::IsDragDropActive()) {
            auto isSource = g.ActiveIdWindow && g.ActiveIdWindow->RootWindow == window;
            if (isSource && !window->Rect().Contains(io.MousePos))
                show = false;
            return;
        }

        if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
            return;

        auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
            ImGui::IsMouseClicked(ImGuiMouseButton_Middle);
        auto hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows |
            ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        if (clicked && !hovered)
            show = false;

        auto focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        if (focused && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            show = false;

    }

}
