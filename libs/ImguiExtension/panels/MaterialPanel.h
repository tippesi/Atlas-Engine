#pragma once

#include "Panel.h"

#include "Material.h"

namespace Atlas::ImguiExtension {

    class MaterialPanel : public Panel {

    public:
        MaterialPanel() : Panel("Material properties") {}

        void Render(Ref<ImguiWrapper>& wrapper, Ref<Material>& material);

    };

}