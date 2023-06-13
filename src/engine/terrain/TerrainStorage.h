#ifndef AE_TERRAINSTORAGE_H
#define AE_TERRAINSTORAGE_H

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
            TerrainStorage() = default;

            /**
             * Constructs a terrain storage object
             * @param rootNodeCount The root node count of the terrain
             * @param LoDCount The level of detail count of the terrain
             * @param sideLength The side length of the terrain
             */
            TerrainStorage(int32_t rootNodeCount, int32_t LoDCount, float sideLength, 
                int32_t materialResolution, int32_t materialCount);

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
             * Makes it possible to write materials afterwards
             */
            void BeginMaterialWrite();

            /**
             *
             * @param slot
             * @param material
             */
            void WriteMaterial(int32_t slot, Ref<Material> material);

            /**
             * Generates all mipmaps after materials have been written
             */
            void EndMaterialWrite();

            /**
             *
             * @param slot
             * @param material
             */
            void RemoveMaterial(int32_t slot, Ref<Material> material);

            /**
             *
             * @return
             * @note Material pointers might be null (empty slots)
             */
            std::vector<Ref<Material>> GetMaterials();

            /**
             * The storage cells the terrain request to change the level of detail.
             */
            std::vector<TerrainStorageCell*> requestedCells;

            /**
             * The storage cells the terrain doesn't need any more because of a change
             * in the level of detail.
             */
            std::vector<TerrainStorageCell*> unusedCells;

            Texture::Texture2DArray baseColorMaps;
            Texture::Texture2DArray roughnessMaps;
            Texture::Texture2DArray aoMaps;
            Texture::Texture2DArray normalMaps;
            Texture::Texture2DArray displacementMaps;

        private:
            void BlitImageToImageArray(Ref<Graphics::Image>& srcImage,
                Ref<Graphics::Image>& dstImage, int32_t slot);

            int32_t rootNodeCount;
            int32_t LoDCount;

            int32_t materialResolution;
            int32_t materialCount;

            int32_t* LoDSideLengths;

            std::vector<Ref<Material>> materials;

            std::vector<std::vector<TerrainStorageCell>> cells;

            Graphics::CommandList* commandList = nullptr;

        };

    }

}

#endif