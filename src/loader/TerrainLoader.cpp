#include "TerrainLoader.h"
#include "AssetLoader.h"

namespace Atlas {

    namespace Loader {

        void TerrainLoader::SaveTerrain(Terrain::Terrain *terrain, std::string filename) {

            auto fileStream = AssetLoader::WriteFile(filename, std::ios::out);

            if (!fileStream.is_open()) {
                throw AtlasException("Couldn't write terrain file");
            }

            // Write file header in ASCII
            std::string header;

            header.append("AET ");
            header.append(std::to_string(terrain->rootNodeSideCount) + " ");
            header.append(std::to_string(terrain->LoDCount) + " ");
            header.append(std::to_string(terrain->patchSizeFactor) + " ");
            header.append(std::to_string(terrain->resolution) + " ");
            header.append(std::to_string(terrain->heightScale) + "\n");

            fileStream << header;

            // Iterate over all LoD level
            for (int32_t i = 0; i < terrain->LoDCount; i++) {

                int32_t cellSideCount = (int32_t)sqrtf((float)terrain->storage->GetCellCount(i));

                for (int32_t x = 0; x < cellSideCount; x++) {
                    for (int32_t y = 0; y < cellSideCount; y++) {

                        auto cell = terrain->storage->GetCell(x, y, i);

                        // Here we assume that all data is present
                        auto data = cell->heightField->GetData();

                        fileStream.write((char*)data.data(), data.size());

                        data = cell->normalMap->GetData();

                        fileStream.write((char*)data.data(), data.size());

                    }
                }

            }

            fileStream.close();

        }

        Terrain::Terrain* TerrainLoader::LoadTerrain(std::string filename) {

            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in);

            if (!fileStream.is_open()) {
                throw AtlasException("Couldn't read terrain file");
            }

            std::string header;

            std::getline(fileStream, header);

            if (header.compare(0, 4, "AET ") != 0) {
                throw AtlasException("File isn't a terrain file");
            }

            auto lastPosition = 4;
            auto position = header.find_first_of(' ', lastPosition);
            int32_t rootNodeSideCount = std::stoi(header.substr(lastPosition, position - lastPosition));

            lastPosition = position;
            position = header.find_first_of(' ', lastPosition + 1);
            int32_t LoDCount = std::stoi(header.substr(lastPosition, position - lastPosition));

            lastPosition = position;
            position = header.find_first_of(' ', lastPosition + 1);
            int32_t patchSizeFactor = std::stoi(header.substr(lastPosition, position - lastPosition));

            lastPosition = position;
            position = header.find_first_of(' ', lastPosition + 1);
            float resolution = std::stof(header.substr(lastPosition, position - lastPosition));

            lastPosition = position;
            position = header.find_first_of(' ', lastPosition + 1);
            float heightScale = std::stof(header.substr(lastPosition, position - lastPosition));

            fileStream.close();

            return new Terrain::Terrain(rootNodeSideCount, LoDCount, patchSizeFactor, resolution, heightScale);

        }

        void TerrainLoader::LoadStorageCell(Terrain::Terrain* terrain, Terrain::TerrainStorageCell* cell,
                std::string filename, bool initWithHeightData) {

            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

            if (!fileStream.is_open()) {
                throw AtlasException("Couldn't read terrain file");
            }

            std::string header;

            std::getline(fileStream, header);

            if (header.compare(0, 4, "AET ") != 0) {
                throw AtlasException("File isn't a terrain file");
            }

            auto tileResolution = 16 * terrain->patchSizeFactor + 1;

            auto nodeDataCount = tileResolution * tileResolution * 5;

            int64_t cellSideCount = (int64_t)sqrtf((float)terrain->storage->GetCellCount(cell->LoD));

            auto nodeCount = (int64_t) ((powf(4.0f, (float) cell->LoD - 1.0f) - 1.0f) / 3.0f)
                    * terrain->rootNodeSideCount * terrain->rootNodeSideCount + cell->x * cellSideCount + cell->y;

            auto cellPosition = nodeDataCount * nodeCount;

            fileStream.seekg(cellPosition, std::ios_base::cur);

            std::vector<uint16_t> heightFieldData(tileResolution * tileResolution);
            std::vector<uint8_t> normalMapData(heightFieldData.size() * 3);

            fileStream.read((char*)heightFieldData.data(), heightFieldData.size() * 2);
            fileStream.read((char*)normalMapData.data(), normalMapData.size());

            cell->heightField = new Texture::Texture2D(tileResolution,
                    tileResolution, AE_R16UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
            cell->normalMap = new Texture::Texture2D(tileResolution,
                    tileResolution, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

            cell->heightField->SetData(heightFieldData);
            cell->normalMap->SetData(normalMapData);

            if (initWithHeightData) {
                cell->heightData.resize(tileResolution * tileResolution);

                for (uint32_t i = 0; i < cell->heightData.size(); i++)
                    cell->heightData[i] = (float)heightFieldData[i];

            }

            fileStream.close();

        }

    }

}