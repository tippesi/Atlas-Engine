#pragma once

#include <string>
#include <imgui.h>
#include <limits>

namespace Atlas::Editor::UI {

    class Popup {

    public:
        Popup(const std::string& name) : name(name), nameID(name + "##" + std::to_string(ID)) {}

        void SetID(const size_t newID) { ID = newID; nameID = name + "##" + std::to_string(ID);  }

        const char* GetNameID() const { return nameID.c_str(); }

        void Open() { ImGui::OpenPopup(GetNameID()); }

        std::string name;

    protected:
        size_t ID = GetID();

        std::string nameID;

    private:
        static size_t GetID() {

            static size_t counter = 0;
            static size_t max = std::numeric_limits<size_t>::max();
            return (counter++) % max;

        }

    };

}