#pragma once

#include <string>
#include <imgui.h>

namespace Atlas::Editor::UI {

    class Popup {

    public:
        Popup(const std::string& name) : name(name), nameID(name + "##" + std::to_string(ID)) {}

        const char* GetNameID() const { return nameID.c_str(); }

        void Open() { ImGui::OpenPopup(GetNameID()); }

        std::string name;

    protected:
        int32_t ID = GetID();

        std::string nameID;

    private:
        static int32_t GetID() {

            static int32_t counter = 0;
            return counter++;

        }

    };

}