#include <algorithm>
#include "TerrainStorage.h"

namespace Atlas {

    namespace Terrain {

        TerrainStorage::TerrainStorage(int32_t rootNodeCount, int32_t LoDCount, float sideLength,
            int32_t materialCount) : rootNodeCount(rootNodeCount), LoDCount(LoDCount) {

            AE_ASSERT(materialCount <= maxMaterialCount && "Too many terrain materials");

            cells.resize(LoDCount);
            LoDSideLengths.resize(LoDCount);

            materials.resize(materialCount);

            for (int32_t i = 0; i < LoDCount; i++) {

                cells[i] = std::vector<TerrainStorageCell>(rootNodeCount * (int32_t)powf(4, (float)i), this);
                LoDSideLengths[i] = (int32_t)sqrtf((float)rootNodeCount * powf(4.0f, (float)i));

                for (int32_t x = 0; x < LoDSideLengths[i]; x++) {
                    for (int32_t y = 0; y < LoDSideLengths[i]; y++) {
                        cells[i][x * LoDSideLengths[i] + y].x = x;
                        cells[i][x * LoDSideLengths[i] + y].y = y;
                        cells[i][x * LoDSideLengths[i] + y].LoD = i;
                        cells[i][x * LoDSideLengths[i] + y].position = vec2((float)x, (float)y) * sideLength;
                    }
                }

                sideLength /= 2.0f;

            }

        }

        TerrainStorageCell* TerrainStorage::GetCell(int32_t x, int32_t y, int32_t LoD) {

            if (x < 0 || x >= LoDSideLengths[LoD] || y < 0 || y >= LoDSideLengths[LoD])
                return nullptr;

            return &cells[LoD][x * LoDSideLengths[LoD] + y];

        }

        int32_t TerrainStorage::GetCellCount(int32_t LoD) {

            return (int32_t)cells[LoD].size();

        }

        void TerrainStorage::WriteMaterial(int32_t slot, ResourceHandle<Material> material) {

            AE_ASSERT(slot >= 0 && slot < maxMaterialCount && "Terrain material slot out of range");
            if (slot < 0 || slot >= maxMaterialCount)
                return;

            if (slot >= int32_t(materials.size()))
                materials.resize(slot + 1);

            materials[slot] = material;

        }

        void TerrainStorage::RemoveMaterial(int32_t slot, ResourceHandle<Material> material) {

            materials[slot].Reset();

        }

        std::vector<ResourceHandle<Material>> TerrainStorage::GetMaterials() {

            return materials;

        }

        void TerrainStorage::PushUnusedCellsToQueue() {

            std::lock_guard lock(unusedCellQueueMutex);

            unusedCellQueue.insert(unusedCells.begin(), unusedCells.end());
            unusedCells.clear();

        }

        std::vector<TerrainStorageCell*> TerrainStorage::GetUnusedCellsQueue() {

            std::lock_guard lock(unusedCellQueueMutex);

            auto wantedCells = std::vector<TerrainStorageCell*>{ unusedCellQueue.begin(), unusedCellQueue.end() };
            unusedCellQueue.clear();

            return wantedCells;

        }

        void TerrainStorage::PushRequestedCellsToQueue() {

            std::lock_guard lock(cellQueueMutex);

            requestedCellQueue.insert(requestedCells.begin(), requestedCells.end());
            requestedCells.clear();

        }

        std::vector<TerrainStorageCell*> TerrainStorage::GetRequestedCellsQueue() {

            std::lock_guard lock(cellQueueMutex);

            auto wantedCells = std::vector<TerrainStorageCell*>{ requestedCellQueue.begin(), requestedCellQueue.end() };
            requestedCellQueue.clear();

            return wantedCells;

        }

        void TerrainStorage::PushRequestedBvhCellsToQueue() {

            std::lock_guard lock(bvhCellQueueMutex);

            requestedBvhCellQueue.insert(requestedBvhCells.begin(), requestedBvhCells.end());
            requestedBvhCells.clear();

        }

        std::vector<TerrainStorageCell*> TerrainStorage::GetRequestedBvhCellsQueue() {

            std::lock_guard lock(bvhCellQueueMutex);

            auto wantedCells = std::vector<TerrainStorageCell*>{ requestedBvhCellQueue.begin(), requestedBvhCellQueue.end() };
            requestedBvhCellQueue.clear();

            return wantedCells;

        }

    }

}