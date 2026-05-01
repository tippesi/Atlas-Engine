#pragma once

#include "Panel.h"
#include "scene/Scene.h"
#include "tools/TerrainGenerator.h"
#include "VegetationGeneratorPanel.h"
#include <ImguiExtension/panels/MaterialsPanel.h>

namespace Atlas::Editor::UI {

    class TerrainPanel : public Panel {

    public:
        enum class TerrainBrushType {
            Height = 0,
            Material,
            Hole
        };

        enum class TerrainBrushFunction {
            Gauss = 0,
            Box,
            Smooth,
            Flatten
        };

        TerrainPanel() : Panel("Terrain") {}

        void Render(ResourceHandle<Terrain::Terrain> terrain, Ref<Scene::Scene> scene);

        bool editingMode = false;
        float brushSize = 1.0f;
        float brushStrength = 1.0f;
        bool brushFlattenQueryRegularGeometry = false;
        TerrainBrushType brushType = TerrainBrushType::Height;
        TerrainBrushFunction brushFunction = TerrainBrushFunction::Gauss;

        int32_t materialBrushSelection = 0;

        TerrainGenerator terrainGenerator;
        VegetationGeneratorPanel vegetationGeneratorPanel;

    private:
        void RenderGeneralSettings(ResourceHandle<Terrain::Terrain>& terrain);

        void RenderMaterialSettings(ResourceHandle<Terrain::Terrain>& terrain);

        void RenderEditingSettings(ResourceHandle<Terrain::Terrain>& terrain, Ref<Scene::Scene> scene);

        void AddTerrainToScene(ResourceHandle<Terrain::Terrain>& terrain, Ref<Scene::Scene>& scene);

        ImguiExtension::MaterialsPanel materialsPanel;

        ResourceSelectionPanel<Terrain::Terrain> terrainSelectionPanel;
        ResourceSelectionPanel<Material> materialSelectionPanel;
        ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;
        
        std::vector<float> LoDDistances;

    };

}