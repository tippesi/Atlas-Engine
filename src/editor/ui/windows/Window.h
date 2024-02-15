#pragma once

#include <stdint.h>
#include <string>

#include <imgui.h>

namespace Atlas::Editor::UI {

    class Window {

    public:
        Window(const std::string& name, bool show) : name(name), nameID(name + "##" + std::to_string(ID)),
            dockSpaceNameID(name + "dockSpace##" + std::to_string(ID)), show(show) {}

        const char* GetNameID() const { return nameID.c_str(); }

        bool Begin(ImGuiWindowFlags flags = 0);

        void End();

        bool show = true;

        std::string name;

        bool inFocus = false;
        bool resetDockingLayout = false;

    protected:
        int32_t ID = GetID();

        std::string nameID;
        std::string dockSpaceNameID;

    private:
        static int32_t GetID() {

            static int32_t counter = 0;
            return counter++;

        }

    };

}