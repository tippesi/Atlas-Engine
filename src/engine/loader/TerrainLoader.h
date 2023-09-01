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
            static Ref<Terrain::Terrain> LoadTerrain(std::string filename);

            /**
             * Stores the terrain in a directory on the hard drive
             * @param terrain
             * @param filename
             * @warning All storage cells of the terrain must be loaded.
             */
            static void SaveTerrain(Ref<Terrain::Terrain> terrain, std::string filename);

            /**
             *
             * @param terrain
             * @param cell
             * @param filename
             * @param initWithHeightData
             */
            static void LoadStorageCell(Ref<Terrain::Terrain> terrain, Terrain::TerrainStorageCell* cell,
                std::string filename, bool initWithHeightData = false);

        private:
            static int32_t ReadInt(const char* ptr, std::string line, size_t& offset);

            static float ReadFloat(const char* ptr, std::string line, size_t& offset);


        };

    }

}