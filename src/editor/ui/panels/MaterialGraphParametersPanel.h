#pragma once

#include "Panel.h"
#include "ResourceSelectionPanel.h"

#include "Material.h"
#include "materialgraph/MaterialGraph.h"

#include <ImguiExtension/panels/MaterialPanel.h>

#include <string>

namespace Atlas::Editor::UI {

    // The parameters of the graph of a material with the values of the material, which overrides the defaults of the graph
    class MaterialGraphParametersPanel : public Panel {

    public:
        MaterialGraphParametersPanel() : Panel("Material graph parameters") {}

        // The parameters, followed by the properties of the material for graphs which don't describe the surface
        void Render(Ref<Material>& material);

        void RenderParameters(Material& material, const MaterialGraph::CompiledGraph& compiled);

    private:
        void RenderParameter(Material& material, const MaterialGraph::Parameter& parameter);

        std::string parameterSearch;
        // Short lists stay open and don't need a search
        static constexpr size_t maxParametersWithoutSearch = 12;

        ImguiExtension::MaterialPanel materialPanel;
        ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;

    };

}
