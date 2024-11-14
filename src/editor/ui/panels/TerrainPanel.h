#pragma once

#include "Panel.h"
#include "scene/Scene.h"
#include "tools/TerrainGenerator.h"
#include <ImguiExtension/panels/MaterialsPanel.h>

namespace Atlas::Editor::UI {

    class TerrainPanel : public Panel {

    public:
        enum class TerrainBrushType {
            HeightGauss = 0,
            HeightBox,
            HeightSmooth,
            Material
        };

        TerrainPanel() : Panel("Terrain") {}

        void Render(ResourceHandle<Terrain::Terrain> terrain, Ref<Scene::Scene> scene);

        bool editingMode = false;
        float brushSize = 1.0f;
        TerrainBrushType brushType = TerrainBrushType::HeightGauss;

        TerrainGenerator terrainGenerator;

    private:
        void RenderGeneralSettings(ResourceHandle<Terrain::Terrain>& terrain);

        void RenderMaterialSettings(ResourceHandle<Terrain::Terrain>& terrain);

        void RenderEditingSettings(ResourceHandle<Terrain::Terrain>& terrain);

        void AddTerrainToScene(ResourceHandle<Terrain::Terrain>& terrain, Ref<Scene::Scene>& scene);

        ImguiExtension::MaterialsPanel materialsPanel;

        ResourceSelectionPanel<Terrain::Terrain> terrainSelectionPanel;
        ResourceSelectionPanel<Material> materialSelectionPanel;
        ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;
        
        std::vector<float> LoDDistances;

    };

}