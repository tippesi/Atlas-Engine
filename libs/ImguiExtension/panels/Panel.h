#pragma once

#include <string>
#include <imgui.h>

#include "../UiElements.h"

namespace Atlas::ImguiExtension {

    class Panel {

    public:
        explicit Panel(const std::string& name) : name(name), nameID(name + "##" + std::to_string(ID)) {}

        const char* GetNameID() const { return nameID.c_str(); }

        std::string name;

        bool isFocused = false;

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