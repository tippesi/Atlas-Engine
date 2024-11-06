#include "TerrainPanel.h"

namespace Atlas::Editor::UI {

    void TerrainPanel::Render(ResourceHandle<Terrain::Terrain> terrain, Ref<Scene::Scene> scene) {

        terrainGenerator.Render();

        auto generatedTerrain = terrainGenerator.GetTerrain();
        if (generatedTerrain != nullptr) {
            scene->terrain = generatedTerrain;

            auto resource = scene->terrain.GetResource();
            ResourceManager<Terrain::Terrain>::AddResource(resource->path, resource);
        }

    }

}