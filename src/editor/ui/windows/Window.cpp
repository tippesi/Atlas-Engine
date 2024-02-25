#include "Window.h"

namespace Atlas::Editor::UI {

    bool Window::Begin(ImGuiWindowFlags flags) {

        if (!show)
            return false;

        if (!ImGui::Begin(GetNameID(), &show, flags)) {
            ImGui::End();
            inFocus = false;
            return false;
        }

        return true;

    }

    void Window::End() {

        ImGui::End();

    }

}