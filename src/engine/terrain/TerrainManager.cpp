#include "TerrainManager.h"

#include "resource/ResourceManager.h"
#include "Terrain.h"
#include "loader/TerrainLoader.h"

namespace Atlas::Terrain {

    JobGroup TerrainManager::loadTerrainCellGroup;

    void TerrainManager::Update() {

#ifdef AE_BINDLESS
        auto loadTerrainCells = [&](JobData) {
            auto terrains = ResourceManager<Terrain>::GetOwnedResources();

            // Note: We could launch one job per terrain
            for (const auto& terrain : terrains) {
                if (!terrain.IsLoaded())
                    continue;

                auto& storage = terrain->storage;

                // Unload old cells first
                auto cells = storage->GetUnusedCellsQueue();
                for (auto cell : cells)                   
                    cell->Unload();

                // Load new cells
                cells = storage->GetRequestedCellsQueue();
                auto span = std::span<TerrainStorageCell*> { cells.begin(), cells.end() };
                Loader::TerrainLoader::LoadStorageCell(terrain.Get(), cells, terrain.GetResource()->path, true);
            }
        };

        if (loadTerrainCellGroup.HasFinished()) {
            JobSystem::Execute(loadTerrainCellGroup, loadTerrainCells);
        }
#endif

    }

}