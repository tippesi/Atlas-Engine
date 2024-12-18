#pragma once

#include "../System.h"
#include "../terrain/Terrain.h"

namespace Atlas {

    namespace Loader {

        class TerrainLoader {

        public:
            /**
             * Loads the terrain.
             * @param filename
             * @return
             * @note This method just loads the terrain information, not the nodes.
             */
            static Ref<Terrain::Terrain> LoadTerrain(const std::string& filename, bool loadNodes = false);

            /**
             * Stores the terrain in a directory on the hard drive
             * @param terrain
             * @param filename
             * @warning All storage cells of the terrain must be loaded.
             */
            static void SaveTerrain(Ref<Terrain::Terrain> terrain, const std::string& filename);

            /**
             *
             * @param terrain
             * @param cell
             * @param filename
             * @param initWithHeightData
             */
            static void LoadStorageCells(Ref<Terrain::Terrain> terrain, std::span<Terrain::TerrainStorageCell*> cell,
                const std::string& filename);

        private:
            static int32_t ReadInt(const char* ptr, std::string line, size_t& offset);

            static float ReadFloat(const char* ptr, std::string line, size_t& offset);


        };

    }

}