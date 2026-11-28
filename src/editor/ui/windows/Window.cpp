#include "Window.h"
#include "../../Theme.h"

#include <Log.h>

#include <imgui_internal.h>

namespace Atlas::Editor::UI {

    bool Window::Begin(ImGuiWindowFlags flags) {

        // A closed window keeps no focus, e.g. undo would still go to it
        if (!show) {
            inFocus = false;
            return false;
        }

        if (requestFocus) {
            ImGui::SetNextWindowFocus();
            requestFocus = false;
        }

        auto visible = ImGui::Begin(GetNameID(), &show, flags);
        // Tabs in the background show the marker as well
        DrawUnsavedMarker(visible);
        if (!visible) {
            ImGui::End();
            inFocus = false;
            return false;
        }

        return true;

    }

    void Window::DrawUnsavedMarker(bool visible) {

        auto window = ImGui::GetCurrentWindow();
        bool docked = window->DockIsActive && window->DockNode && window->DockNode->HostWindow;
        if (!hasUnsavedChanges || label.empty() || (!visible && !docked))
            return;

        // A title has only one color, the marker goes into the room after the label
        const auto& style = ImGui::GetStyle();
        auto titleRect = docked ? window->DC.DockTabItemRect : window->TitleBarRect();
        auto drawList = docked ? window->DockNode->HostWindow->DrawList : window->DrawList;
        // Undocked windows have the collapse button in front of the label
        auto textOffset = docked ? style.FramePadding.x : style.FramePadding.x + ImGui::GetFontSize() + style.ItemInnerSpacing.x;
        auto markerPos = ImVec2(titleRect.Min.x + textOffset + ImGui::CalcTextSize(label.c_str()).x + 0.5f * style.ItemInnerSpacing.x,
            titleRect.Min.y + style.FramePadding.y);

        drawList->PushClipRect(titleRect.Min, titleRect.Max, false);
        drawList->AddText(markerPos, ImGui::GetColorU32(Theme::Error()), "*");
        drawList->PopClipRect();

    }

    void Window::SetTitle(const std::string& label, const std::string& id) {

        auto window = !titleID.empty() && titleID != id ? ImGui::FindWindowByName(GetNameID()) : nullptr;
        if (window) {
            if (window->DockId) {
                ImGui::SetNextWindowDockID(window->DockId, ImGuiCond_Always);
            }
            else {
                ImGui::SetNextWindowPos(window->Pos);
                ImGui::SetNextWindowSize(window->Size);
            }
            requestFocus |= inFocus;
        }

        this->label = label;
        titleID = id;
        nameID = label + (hasUnsavedChanges ? "   ###" : "###") + id;

    }

    void Window::End() {

        ImGui::End();

    }

}