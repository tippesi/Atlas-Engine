#pragma once

#include <stdint.h>
#include <string>

#include <imgui.h>

namespace Atlas::Editor::UI {

    class Window {

    public:
        Window(const std::string& name, bool show) : name(name), nameID(name + "##" + std::to_string(ID)),
            dockSpaceNameID(name + "dockSpace##" + std::to_string(ID)), show(show) {}

        virtual ~Window() = default;

        const char* GetNameID() const { return nameID.c_str(); }

        bool Begin(ImGuiWindowFlags flags = 0);

        void End();

        bool show = true;

        std::string name;

        bool inFocus = false;
        bool resetDockingLayout = false;
        // Docks the window as a tab into the dock node, or next to the scenes without one, when it is rendered the next time
        bool requestDocking = false;
        ImGuiID dockNodeID = 0;
        bool requestFocus = false;
        // Shown as a red star after the label of the tab or title bar
        bool hasUnsavedChanges = false;
        // Whether the window was shown in the last frame, a window that opens becomes a tab
        bool wasShown = false;

    protected:
        // The label and an id which stays when the label changes, e.g. the path of a file. Call before Begin
        void SetTitle(const std::string& label, const std::string& id);

        void DrawUnsavedMarker(bool visible);

        int32_t ID = GetID();
        std::string label;

        std::string nameID;
        std::string dockSpaceNameID;

    private:
        static int32_t GetID() {

            static int32_t counter = 0;
            return counter++;

        }

    };

}