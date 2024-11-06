#pragma once

#include "Panel.h"
#include "scene/Scene.h"
#include "tools/TerrainGenerator.h"
#include <ImguiExtension/panels/MaterialsPanel.h>

namespace Atlas::Editor::UI {

    class TerrainPanel : public Panel {

    public:
        TerrainPanel() : Panel("Terrain") {}

        void Render(ResourceHandle<Terrain::Terrain> terrain, Ref<Scene::Scene> scene);

    private:
        void RenderGeneralSettings(ResourceHandle<Terrain::Terrain>& terrain);

        void RenderMaterialSettings(ResourceHandle<Terrain::Terrain>& terrain);

        void RenderEditingSettings(ResourceHandle<Terrain::Terrain>& terrain);

        void AddTerrainToScene(Ref<Terrain::Terrain>& terrain, Ref<Scene::Scene>& scene);

        TerrainGenerator terrainGenerator;

        ImguiExtension::MaterialsPanel materialsPanel;

        ResourceSelectionPanel<Terrain::Terrain> terrainSelectionPanel;
        ResourceSelectionPanel<Material> materialSelectionPanel;
        ResourceSelectionPanel<Texture::Texture2D> textureSelectionPanel;

    };

}