#pragma once

#include "Panel.h"

#include "MaterialPanel.h"
#include "resource/Resource.h"

#include <string>

namespace Atlas::ImguiExtension {

    class MaterialsPanel : public Panel {

        using MaterialSelector = std::optional<std::function<ResourceHandle<Material>(ResourceHandle<Material>)>>;
        // Renders a material with a graph instead of the properties, e.g. with the parameters of the graph
        using GraphMaterialRenderer = std::optional<std::function<void(Ref<Material>&)>>;

    public:
        MaterialsPanel() : Panel("Materials properties") {}

        void Render(Ref<ImguiWrapper>& wrapper, std::vector<ResourceHandle<Material>>& materials, 
            MaterialSelector materialSelector = {}, MaterialPanel::TextureSelector textureSelector = {},
            GraphMaterialRenderer graphMaterialRenderer = {});

        void Render(Ref<ImguiWrapper>& wrapper, std::vector<Ref<Material>>& materials, 
            MaterialPanel::TextureSelector textureSelector = {});

    private:
        MaterialPanel materialPanel;

        std::string materialSearch;

    };

}