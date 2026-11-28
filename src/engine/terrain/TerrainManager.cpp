#include "TerrainManager.h"

#include "resource/ResourceManager.h"
#include "Terrain.h"
#include "loader/TerrainLoader.h"

namespace Atlas::Terrain {

    std::atomic_bool TerrainManager::enable = true;
    JobGroup TerrainManager::loadTerrainCellGroup{ "Terrain manager update" };

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
                // Streaming is the loading boundary of the cells, a broken file leaves them unloaded
                try {
                    Loader::TerrainLoader::LoadStorageCells(terrain.Get(), cells, terrain.GetResource()->path);
                }
                catch (const ResourceLoadException& exception) {
                    Log::Error("Couldn't stream terrain cells: " + std::string(exception.what()));
                }
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
