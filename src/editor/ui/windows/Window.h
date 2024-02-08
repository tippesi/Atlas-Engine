#pragma once

#include <stdint.h>
#include <string>

#include <imgui.h>

namespace Atlas::Editor::UI {

    class Window {

    public:
        explicit Window(const std::string& name) : name(name), nameID(name + "##" + std::to_string(ID)),
            dockSpaceNameID(name + "dockSpace##" + std::to_string(ID)) {}

        const char* GetNameID() const { return nameID.c_str(); }

        std::string name;

        bool inFocus = false;
        bool resetDockingLayout = true;

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