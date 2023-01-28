#include "TerrainLoader.h"
#include "AssetLoader.h"
#include "MaterialLoader.h"
#include "../Log.h"

#include "../common/Path.h"

namespace Atlas {

    namespace Loader {

        void TerrainLoader::SaveTerrain(Terrain::Terrain *terrain, std::string filename) {

            auto fileStream = AssetLoader::WriteFile(filename, std::ios::out | std::ios::binary);

            if (!fileStream.is_open()) {
                Log::Error("Couldn't write terrain file " + filename);
				return;
            }

			auto materials = terrain->storage->GetMaterials();

			// There don't have to be all materials
			int32_t count = 0;
			for (auto material : materials)
				if (material)
					count++;

            // Write file header in ASCII
            std::string header, body;

			header.append("AET ");
			header.append(std::to_string(count) + " ");
            header.append(std::to_string(terrain->rootNodeSideCount) + " ");
            header.append(std::to_string(terrain->LoDCount) + " ");
            header.append(std::to_string(terrain->patchSizeFactor) + " ");
            header.append(std::to_string(terrain->resolution) + " ");
            header.append(std::to_string(terrain->heightScale) + " ");
			header.append(std::to_string(terrain->bakeResolution) + "\n");

            fileStream << header;

			body.append(std::to_string(terrain->tessellationFactor) + " ");
			body.append(std::to_string(terrain->tessellationSlope) + " ");
			body.append(std::to_string(terrain->tessellationShift) + " ");
			body.append(std::to_string(terrain->maxTessellationLevel) + " ");
			body.append(std::to_string(terrain->displacementDistance) + "\n");

			// Write all informations about the LoDs
			for (int32_t i = 0; i < terrain->LoDCount; i++) {
				body.append(std::to_string(terrain->GetLoDDistance(i)));
				if (i + 1 == terrain->LoDCount)
					body.append("\n");
				else
					body.append(" ");
			}

			// Write all material paths and store the materials
			auto terrainDir = Common::Path::GetDirectory(filename);
			auto materialDir = terrainDir + "/material";

			AssetLoader::MakeDirectory(materialDir);

			count = 0;
			for (auto& material : materials) {
				if (material) {
					auto filename = materialDir + "/" + material->name + ".aematerial";
					MaterialLoader::SaveMaterial(material, filename);
					body.append(std::to_string(count) + " material/" + material->name + ".aematerial" + "\n");
				}
				count++;
			}

			fileStream << body;

            // Iterate over all LoD level
            for (int32_t i = 0; i < terrain->LoDCount; i++) {

                int32_t cellSideCount = (int32_t)sqrtf((float)terrain->storage->GetCellCount(i));

				auto isLeaf = i == terrain->LoDCount - 1;

                for (int32_t x = 0; x < cellSideCount; x++) {
                    for (int32_t y = 0; y < cellSideCount; y++) {

                        auto cell = terrain->storage->GetCell(x, y, i);

						if (isLeaf) {
							// fileStream.write((char*)cell->materialIndices, sizeof(cell->materialIndices));
						}

                        // Here we assume that all cells are present
                        auto heightData = cell->heightField->GetData<uint16_t>();

                        fileStream.write((char*)heightData.data(), heightData.size() * 2);

                        auto data = cell->normalMap->GetData<uint8_t>();

                        fileStream.write((char*)data.data(), data.size());

						data = cell->splatMap->GetData<uint8_t>();

						fileStream.write((char*)data.data(), data.size());

                    }
                }

            }

            fileStream.close();

        }

        Terrain::Terrain* TerrainLoader::LoadTerrain(std::string filename) {

            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in);

            if (!fileStream.is_open()) {
                Log::Error("Couldn't read terrain file " + filename);
				return nullptr;
            }

            std::string header, line;

            std::getline(fileStream, header);

            if (header.compare(0, 4, "AET ") != 0) {
                Log::Error("File isn't a terrain file " + filename);
				return nullptr;
            }

			size_t offset = 4;
			auto materialCount = ReadInt(" ", header, offset);
			auto rootNodeSideCount = ReadInt(" ", header, offset);
			auto LoDCount = ReadInt(" ", header, offset);
			auto patchSizeFactor = ReadInt(" ", header, offset);
			auto resolution = ReadFloat(" ", header, offset);
			auto heightScale = ReadFloat(" ", header, offset);
			auto bakeResolution = ReadInt("\r\n", header, offset);

			std::getline(fileStream, line);

			offset = 0;
			auto tessFactor = ReadFloat(" ", line, offset);
			auto tessSlope = ReadFloat(" ", line, offset);
			auto tessShift = ReadFloat(" ", line, offset);
			auto tessMaxLevel = ReadFloat(" ", line, offset);
			auto displacementDistance = ReadFloat("\r\n", line, offset);

			auto terrain = new Terrain::Terrain(rootNodeSideCount, LoDCount, 
				patchSizeFactor, resolution, heightScale);

			terrain->SetTessellationFunction(tessFactor, tessSlope, tessShift, tessMaxLevel);
			terrain->SetDisplacementDistance(displacementDistance);

			terrain->bakeResolution = bakeResolution;

			std::getline(fileStream, line);

			offset = 0;
			for (int32_t i = 0; i < LoDCount; i++) {
				float distance = 0.0f;
				if (i == LoDCount - 1)
					distance = ReadFloat("\r\n", line, offset);
				else
					distance = ReadFloat(" ", line, offset);
				terrain->SetLoDDistance(i, distance);
			}

			auto terrainDir = Common::Path::GetDirectory(filename);

			for (int32_t i = 0; i < materialCount; i++) {
				std::getline(fileStream, line);

				size_t offset = 0;
				auto slot = ReadInt(" ", line, offset);

				auto pos = line.find_last_of("\r\n");
				auto materialPath = terrainDir + "/" + line.substr(offset, pos - offset);
				auto material = MaterialLoader::LoadMaterial(materialPath, 1024);

				if (material)
					terrain->storage->AddMaterial(slot, material);
			}

			fileStream.close();

			terrain->filename = filename;

            return terrain;

        }

        void TerrainLoader::LoadStorageCell(Terrain::Terrain* terrain, Terrain::TerrainStorageCell* cell,
                std::string filename, bool initWithHeightData) {

            auto fileStream = AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

			if (!fileStream.is_open()) {
				Log::Error("Couldn't read terrain file " + filename);
				return;
			}

            std::string header, body;

            std::getline(fileStream, header);

			if (header.compare(0, 4, "AET ") != 0) {
				Log::Error("File isn't a terrain file " + filename);
				return;
			}

			auto position = header.find_first_of(' ', 4);
			int32_t materialCount = std::stoi(header.substr(4, position - 4));

			// Skip the body
			for (int32_t i = 0; i < materialCount + 2; i++)
				std::getline(fileStream, body);

			auto isLeaf = cell->LoD == terrain->LoDCount - 1;
            auto tileResolution = 8 * terrain->patchSizeFactor + 1;

			// Height map + splat map
			auto nodeDataCount = (int64_t)tileResolution * tileResolution * 3;

            int64_t cellSideCount = (int64_t)sqrtf((float)terrain->storage->GetCellCount(cell->LoD));

			auto downsample = (int32_t)powf(2.0f, (float)terrain->LoDCount - 1.0f);
			auto tileSideCount = (int64_t)sqrtf((float)terrain->storage->GetCellCount(0));
			auto normalDataResolution = int64_t(0);

			auto currPos = int64_t(0);

			// Different resolutions for each LoD
			for (int32_t i = 0; i <= cell->LoD; i++) {
				auto sizeFactor = int64_t(glm::min(downsample, 
					terrain->bakeResolution / (tileResolution - 1)));
				normalDataResolution = int64_t(tileResolution - 1) * sizeFactor + 3;
				auto nodeSize = nodeDataCount + normalDataResolution
					* normalDataResolution * 3;

				if (cell->LoD == i) {
					currPos += (cell->x * tileSideCount + cell->y) * nodeSize;
					break;
				}
				
				currPos += tileSideCount * tileSideCount * nodeSize;

				downsample /= 2;
				tileSideCount *= 2;
			}

            fileStream.seekg(currPos, std::ios_base::cur);

			std::vector<uint16_t> heightFieldData(tileResolution * tileResolution);
            fileStream.read((char*)heightFieldData.data(), heightFieldData.size() * 2);
            // cell->heightField = new Texture::Texture2D(tileResolution,
            // 	tileResolution, AE_R16UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
			cell->heightField->SetData(heightFieldData);

			std::vector<uint8_t> normalMapData(normalDataResolution * 
				normalDataResolution * 3);
            fileStream.read((char*)normalMapData.data(), normalMapData.size());
            // cell->normalMap = new Texture::Texture2D(normalDataResolution,
            // 	normalDataResolution, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR, true, true);
			cell->normalMap->SetData(normalMapData);

			std::vector<uint8_t> splatMapData(heightFieldData.size());
			fileStream.read((char*)splatMapData.data(), splatMapData.size());
            // cell->splatMap = new Texture::Texture2D(tileResolution,
            // 		tileResolution, AE_R8UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
			cell->splatMap->SetData(splatMapData);
			
			if (initWithHeightData) {
                cell->heightData.resize(tileResolution * tileResolution);

                for (uint32_t i = 0; i < cell->heightData.size(); i++)
                    cell->heightData[i] = (float)heightFieldData[i] / 65535.0f;
            }

            fileStream.close();

        }

		int32_t TerrainLoader::ReadInt(const char* ptr, std::string line, size_t& offset) {

			auto currOffset = offset;
			offset = line.find_first_of(ptr, currOffset + 1) + 1;
			return std::stoi(line.substr(currOffset, offset - currOffset - 1));

		}

		float TerrainLoader::ReadFloat(const char* ptr, std::string line, size_t& offset) {

			auto currOffset = offset;
			offset = line.find_first_of(ptr, currOffset + 1) + 1;
			return std::stof(line.substr(currOffset, offset - currOffset - 1));

		}

    }

}