#pragma once

#include "Panel.h"
#include "tools/VegetationGenerator.h"

namespace Atlas::Editor::UI {

    class VegetationGeneratorPanel : public Panel {

    public:
        VegetationGeneratorPanel() : Panel("VegetationGenerator") {}

        void Render(Ref<Scene::Scene>& scene, TerrainGenerator& terrainGenerator);

        VegetationGenerator vegetationGenerator;

    private:
        void RenderBiomeVegetationTypes(Ref<Scene::Scene>& scene,
            Ref<Terrain::Terrain>& terrain, TerrainGenerator& terrainGenerator);

        UI::ResourceSelectionPanel<Mesh::Mesh> meshSelectionPanel;

    };

}