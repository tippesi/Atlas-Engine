#pragma once

#include "../System.h"
#include "TerrainStorageCell.h"

#include <vector>

namespace Atlas {

    namespace Terrain {

        /**
         * Manages the materials and data for all terrain nodes.
         */
        class TerrainStorage {

        public:
            // The size of the material array of the terrain shaders
            static constexpr int32_t maxMaterialCount = 128;

            TerrainStorage() = default;

            /**
             * Constructs a terrain storage object
             * @param rootNodeCount The root node count of the terrain
             * @param LoDCount The level of detail count of the terrain
             * @param sideLength The side length of the terrain
             */
            TerrainStorage(int32_t rootNodeCount, int32_t LoDCount, float sideLength, int32_t materialCount);

            /**
             * Finds the cell to the corresponding arguments in the storage object
             * @param x The x sector of the node
             * @param y The y sector of the node
             * @param LoD The level of detail of the node
             * @return A pointer to a TerrainStorageCell
             * @remarks The terrain storage can be understood as a mipmap where each
             * level of detail is represented by a mipmap level. This means y is the
             * coordinate in z direction of the terrain. Also x and y aren't world space
             * coordinates but are the coordinates of a specific terrain node in their level of detail.
             * E.g at a level of detail of 5 and a rootNodeCount of 9 x and y are in range (0, 2^(5-1) * 9)
             */
            TerrainStorageCell* GetCell(int32_t x, int32_t y, int32_t LoD);

            /**
             * Returns the number of cell for a specific LoD level
             * @param LoD The LoD level
             * @return An integer with the number of cells
             */
            int32_t GetCellCount(int32_t LoD);

            /**
             *
             * @param slot
             * @param material
             */
            void WriteMaterial(int32_t slot, ResourceHandle<Material> material);

            /**
             *
             * @param slot
             * @param material
             */
            void RemoveMaterial(int32_t slot, ResourceHandle<Material> material);

            void PushUnusedCellsToQueue();

            std::vector<TerrainStorageCell*> GetUnusedCellsQueue();

            void PushRequestedCellsToQueue();
            
            std::vector<TerrainStorageCell*> GetRequestedCellsQueue();
            
            void PushRequestedBvhCellsToQueue();

            std::vector<TerrainStorageCell*> GetRequestedBvhCellsQueue();

            /**
             *
             * @return
             * @note Material pointers might be null (empty slots)
             */
            std::vector<ResourceHandle<Material>> GetMaterials();

            std::vector<TerrainStorageCell*> unusedCells;
            std::vector<TerrainStorageCell*> requestedCells;
            std::vector<TerrainStorageCell*> requestedBvhCells;

            std::set<TerrainStorageCell*> unusedCellQueue;
            std::set<TerrainStorageCell*> requestedCellQueue;
            std::set<TerrainStorageCell*> requestedBvhCellQueue;           

            std::vector<ResourceHandle<Material>> materials;
            std::vector<std::vector<TerrainStorageCell>> cells;

            bool inEditing = false;

        private:
            int32_t rootNodeCount;
            int32_t LoDCount;

            std::vector<int32_t> LoDSideLengths;

            std::mutex unusedCellQueueMutex;
            std::mutex cellQueueMutex;
            std::mutex bvhCellQueueMutex;

        };

    }

}