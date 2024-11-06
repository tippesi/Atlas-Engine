#pragma once

#include "Panel.h"
#include "scene/Scene.h"
#include "tools/TerrainGenerator.h"

namespace Atlas::Editor::UI {

    class TerrainPanel : public Panel {

    public:
        TerrainPanel() : Panel("Terrain") {}

        void Render(ResourceHandle<Terrain::Terrain> terrain, Ref<Scene::Scene> scene);

    private:
        TerrainGenerator terrainGenerator;

    };

}