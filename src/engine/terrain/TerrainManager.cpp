#include "TerrainManager.h"

#include "resource/ResourceManager.h"
#include "Terrain.h"
#include "loader/TerrainLoader.h"

namespace Atlas::Terrain {

    bool TerrainManager::enable = true;
    JobGroup TerrainManager::loadTerrainCellGroup;

    void TerrainManager::Update() {

        auto loadTerrainCells = [&](JobData) {
            auto terrains = ResourceManager<Terrain>::GetOwnedResources();

            // Note: We could launch one job per terrain
            for (const auto& terrain : terrains) {
                if (!terrain.IsLoaded())
                    continue;

                auto& storage = terrain->storage;
                if (!storage)
                    continue;

                // Unload old cells first
                auto cells = storage->GetUnusedCellsQueue();
                for (auto cell : cells)                   
                    cell->Unload();

                // Load new cells
                cells = storage->GetRequestedCellsQueue();
                auto span = std::span<TerrainStorageCell*> { cells.begin(), cells.end() };
                Loader::TerrainLoader::LoadStorageCells(terrain.Get(), cells, terrain.GetResource()->path);
            }
        };

        if (loadTerrainCellGroup.HasFinished() && enable) {
            JobSystem::Execute(loadTerrainCellGroup, loadTerrainCells);
        }

    }

    void TerrainManager::WaitForJobCompletion() {

        JobSystem::Wait(loadTerrainCellGroup);

    }

}