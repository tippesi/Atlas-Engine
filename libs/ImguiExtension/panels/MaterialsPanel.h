#pragma once

#include "Panel.h"

#include "MaterialPanel.h"

#include <string>

namespace Atlas::ImguiExtension {

    class MaterialsPanel : public Panel {

    public:
        MaterialsPanel() : Panel("Materials properties") {}

        void Render(Ref<ImguiWrapper>& wrapper, std::vector<Ref<Material>>& materials);

    private:
        MaterialPanel materialPanel;

        std::string materialSearch;

    };

}