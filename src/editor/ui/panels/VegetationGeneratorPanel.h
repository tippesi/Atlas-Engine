#pragma once

#include "Panel.h"
#include "tools/VegetationGenerator.h"

namespace Atlas::Editor::UI {

    class VegetationGeneratorPanel : public Panel {

    public:
        VegetationGeneratorPanel() : Panel("VegetationGenerator") {}

        void Render(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator);

        VegetationGenerator vegetationGenerator;
        std::unordered_map<size_t, std::vector<VegetationGenerator::VegetationType>> biomeToVegetationType;

    private:
        void RenderBiomeVegetationTypes(std::vector<VegetationGenerator::VegetationType>& types);

        std::vector<VegetationGenerator::VegetationType>& GetVegetationTypes(size_t id);

        UI::ResourceSelectionPanel<Mesh::Mesh> meshSelectionPanel;

    };

}