#include "TerrainTool.h"
#include "../loader/AssetLoader.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"
#include "libraries/stb/stb_image_resize.h"

#include <string>
#include <sys/stat.h>

namespace Atlas {

	namespace Tools {

		Terrain::Terrain* TerrainTool::GenerateTerrain(Loader::Image16 &heightImage, int32_t rootNodeSideCount, int32_t LoDCount,
											  int32_t patchSize, float resolution, float height) {

			// Check if everything is correct
			int32_t maxNodesPerSide = (int32_t)powf(2.0f, (float)LoDCount - 1.0f) * rootNodeSideCount;

			if (heightImage.height != heightImage.width || heightImage.channels != 1) {
				throw AtlasException("Some input value wasn't following the specifications");
			}

			auto terrain = new Terrain::Terrain(rootNodeSideCount, LoDCount, patchSize, resolution, height);

			// Calculate the number of vertices per tile and resize the height data to map 1:1
			int32_t tileResolution = 16 * patchSize;

			int32_t totalResolution = tileResolution * maxNodesPerSide;

			std::vector<uint16_t> heightData(totalResolution * totalResolution);

			stbir_resize_uint16_generic(heightImage.data.data(), heightImage.width, heightImage.height,
										heightImage.width * 2, heightData.data(), totalResolution,
										totalResolution, totalResolution * 2, 1, -1, 0,
										STBIR_EDGE_CLAMP,
										STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);

			// We need some room in the bottom and right for overlapping vertices
			tileResolution += 1;
			int32_t tileResolutionSquared = tileResolution * tileResolution;

			auto storage = terrain->storage;

			std::vector<uint16_t> cellHeightData(tileResolutionSquared);

			// i is in x direction, j in y direction
			for (int32_t i = 0; i < maxNodesPerSide; i++) {
				for (int32_t j = 0; j < maxNodesPerSide; j++) {
					auto cell = terrain->storage->GetCell(i, j, LoDCount - 1);

					// Create the data structures for the cell
					cell->heightData.resize(tileResolutionSquared);
					cell->heightField = new Texture::Texture2D(tileResolution, 
						tileResolution, AE_R16UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
					cell->normalMap = new Texture::Texture2D(tileResolution, 
						tileResolution, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

					// Now copy a tile of the original image
					// We make sure that every tile has the same size
					// We also increased resolution to make sure that
					// adjacent cells connect to each other
					for (int32_t y = 0; y < tileResolution; y++) {
						for (int32_t x = 0; x < tileResolution; x++) {
							int32_t cellOffset = y * tileResolution + x;
							int32_t xImage = i * (tileResolution - 1) + x;
							int32_t yImage = j * (tileResolution - 1) + y;
							int32_t imageOffset = 0;
							if (xImage >= totalResolution && yImage >= totalResolution) {
								imageOffset = (totalResolution - 1) * totalResolution + totalResolution - 1;
							}
							else if (yImage >= totalResolution) {
								imageOffset = (totalResolution - 1) * totalResolution + xImage;
							}
							else if (xImage >= totalResolution) {
								imageOffset = yImage * totalResolution + totalResolution - 1;
							}
							else {
								imageOffset = yImage * totalResolution + xImage;
							}
							cell->heightData[cellOffset] = (float)heightData[imageOffset];
							cellHeightData[cellOffset] = heightData[imageOffset];
						}
					}

					cell->heightField->SetData(cellHeightData);

				}
			}

			BakeTerrain(terrain);

			return terrain;

		}

		void TerrainTool::SaveTerrain(Terrain::Terrain *terrain, std::string filename) {

			auto fileStream = Loader::AssetLoader::WriteFile(filename, std::ios::out);

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

		Terrain::Terrain* TerrainTool::LoadTerrain(std::string filename) {

			auto fileStream = Loader::AssetLoader::ReadFile(filename, std::ios::in);

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

		void TerrainTool::LoadStorageCell(Terrain::Terrain* terrain, Terrain::TerrainStorageCell* cell,
				std::string filename, bool initWithHeightData) {

			auto fileStream = Loader::AssetLoader::ReadFile(filename, std::ios::in | std::ios::binary);

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

                for (int32_t i = 0; i < cell->heightData.size(); i++)
                    cell->heightData[i] = (float)heightFieldData[i];

            }

			fileStream.close();

		}

		void TerrainTool::BakeTerrain(Terrain::Terrain *terrain) {

			// Generate one large heightmap (assumes all tiles have the same size)
			int32_t tileResolution = terrain->storage->GetCell(0, 0, terrain->LoDCount - 1)->heightField->width;
			int32_t tileResolutionSquared = tileResolution * tileResolution;
			int32_t tileCount = terrain->storage->GetCellCount(terrain->LoDCount - 1);

			int32_t tileSideCount = (int32_t)sqrtf((float)tileCount);

			auto heightData = std::vector<uint16_t>(tileCount * tileResolution * tileResolution);

			int32_t heightDataResolution = (int32_t)sqrtf((float)heightData.size());

			// i is in x direction, j in y direction
			for (int32_t i = 0; i < tileSideCount; i++) {
				for (int32_t j = 0; j < tileSideCount; j++) {
					auto cell = terrain->storage->GetCell(i, j, terrain->LoDCount - 1);

					// Now copy a tile of the original image
					// We make sure that every tile has the same size
					// We also increased resolution to make sure that
					// adjacent cells connect to each other
					for (int32_t y = 0; y < tileResolution; y++) {
						for (int32_t x = 0; x < tileResolution; x++) {
							int32_t cellOffset = y * tileResolution + x;
							int32_t imageOffset = (j * (tileResolution - 1) + y) * heightDataResolution +
												  i * (tileResolution - 1) + x;
							heightData[imageOffset] = cell->heightData[cellOffset];
						}
					}
				}
			}

			// Calculate the normals
			auto normalData = std::vector<uint8_t>(heightData.size() * 3);

			GenerateNormalData(heightData, normalData, (tileResolution - 1) * tileSideCount + 1,
							   (tileResolution - 1) * tileSideCount + 1, 8.0f);

			// Iterate through all the LoD level and resize images according to the tile size

			std::vector<uint16_t> tileHeightData(tileResolution * tileResolution);
			std::vector<uint8_t> tileNormalData(tileHeightData.size() * 3);

			for (int32_t k = 0; k < terrain->LoDCount; k++) {

				int32_t Lod = terrain->LoDCount - k - 1;

				int32_t tileSideCountLod = tileSideCount / (int32_t)powf(2.0f, (float)k);
				int32_t tileCountLod = terrain->storage->GetCellCount(Lod);

				std::vector<uint16_t> resizedHeightData(tileSideCountLod * tileSideCountLod * tileResolutionSquared);
				std::vector<uint8_t> resizedNormalData(resizedHeightData.size() * 3);

				int32_t resizedHeightDataResolution = (int32_t)sqrtf((float)resizedHeightData.size());

				if (k == 0) {
					resizedHeightData = heightData;
					resizedNormalData = normalData;
				}
				else {
					// Downsample data
					stbir_resize_uint16_generic(heightData.data(), heightDataResolution, heightDataResolution,
												heightDataResolution * 2, resizedHeightData.data(), resizedHeightDataResolution,
												resizedHeightDataResolution, resizedHeightDataResolution * 2, 1, -1, 0,
												STBIR_EDGE_CLAMP,
												STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);

					stbir_resize_uint8_generic(normalData.data(), heightDataResolution, heightDataResolution,
											   heightDataResolution * 3, resizedNormalData.data(), resizedHeightDataResolution,
											   resizedHeightDataResolution, resizedHeightDataResolution * 3, 3, -1, 0,
											   STBIR_EDGE_CLAMP,
											   STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
				}

				// i is in x direction, j in y direction
				for (int32_t i = 0; i < tileSideCountLod; i++) {
					for (int32_t j = 0; j < tileSideCountLod; j++) {
						auto cell = terrain->storage->GetCell(i, j, Lod);

						// Now copy a tile of the original image
						// We make sure that every tile has the same size
						// We also increased resolution to make sure that
						// adjacent cells connect to each other
						for (int32_t y = 0; y < tileResolution; y++) {
							for (int32_t x = 0; x < tileResolution; x++) {
								int32_t cellOffset = y * tileResolution + x;
								int32_t imageOffset = (j * (tileResolution - 1) + y) * resizedHeightDataResolution +
													  i * (tileResolution - 1) + x;
								tileHeightData[cellOffset] = resizedHeightData[imageOffset];
								tileNormalData[cellOffset * 3] = resizedNormalData[imageOffset * 3];
								tileNormalData[cellOffset * 3 + 1] = resizedNormalData[imageOffset * 3 + 1];
								tileNormalData[cellOffset * 3 + 2] = resizedNormalData[imageOffset * 3 + 2];
							}
						}

						cell->heightField->SetData(tileHeightData);
						cell->normalMap->SetData(tileNormalData);

					}
				}

			}


		}

		void TerrainTool::BrushHeight(Terrain::Terrain *terrain, Kernel *kernel, float scale, vec2 position) {

			int32_t LoD = terrain->LoDCount - 1;

			// Get the storage cells
			auto middleMiddle = terrain->GetStorageCell(position.x, position.y, LoD);

			if (middleMiddle == nullptr)
				return;

			auto upperLeft = terrain->storage->GetCell(middleMiddle->x - 1, middleMiddle-> y - 1, LoD);
			auto upperMiddle = terrain->storage->GetCell(middleMiddle->x, middleMiddle->y - 1, LoD);
			auto upperRight = terrain->storage->GetCell(middleMiddle->x + 1, middleMiddle->y - 1, LoD);
			auto middleLeft = terrain->storage->GetCell(middleMiddle->x - 1, middleMiddle->y, LoD);
			auto middleRight = terrain->storage->GetCell(middleMiddle->x + 1, middleMiddle->y, LoD);
			auto bottomLeft = terrain->storage->GetCell(middleMiddle->x - 1, middleMiddle->y + 1, LoD);
			auto bottomMiddle = terrain->storage->GetCell(middleMiddle->x, middleMiddle->y + 1, LoD);
			auto bottomRight = terrain->storage->GetCell(middleMiddle->x + 1, middleMiddle->y + 1, LoD);

			Terrain::TerrainStorageCell* cells[] = {upperLeft, upperMiddle, upperRight,
										   middleLeft, middleMiddle, middleRight,
										   bottomLeft, bottomMiddle, bottomRight};

			// Now bring all height data into one array (we assume that all tiles have the same size)
			int32_t width = middleMiddle->heightField->width - 1;
			int32_t height = middleMiddle->heightField->height - 1;

			std::vector<float> heights(width * height * 9);

			auto data = heights.data();

			for (int32_t i = 0; i < 3; i++) {
				for (int32_t j = 0; j < 3; j++) {

					auto cell = cells[i * 3 + j];

					if (cell == nullptr)
						continue;

					for (int32_t k = 0; k < height + 1; k++) {
						for (int32_t l = 0; l < width + 1; l++) {
							int32_t x = j * width + l;
							int32_t y = i * height + k;
							// We don't want to update the last heights of the right and bottom cells
							if (x >= width * 3 || y >= height * 3)
								continue;

							int32_t dataOffset = y * 3 * width + x;
							int32_t cellOffset = k * (width + 1) + l;

							data[dataOffset] = cell->heightData[cellOffset];

						}
					}

				}
			}

			// Apply the kernel on the whole data
			position -= middleMiddle->position;

			int32_t x = (int32_t)floorf(position.x / terrain->resolution);
			int32_t y = (int32_t)floorf(position.y / terrain->resolution);

			x += width;
			y += height;

			std::vector<std::vector<float>>* weights = nullptr;
			std::vector<std::vector<ivec2>>* offsets = nullptr;

			kernel->Get(weights, offsets);

			for (int32_t i = 0; i < weights->size(); i++) {
				for (int32_t j = 0; j < weights->size(); j++) {
					int32_t xTranslated = x + (*offsets)[i][j].x;
					int32_t yTranslated = y + (*offsets)[i][j].y;
					int32_t index = yTranslated * width * 3 + xTranslated;
					float value = data[index] + scale * (*weights)[i][j] / terrain->heightScale * 65535.0f;
					data[index] = glm::clamp(value, 0.0f, 65535.0f);
				}
			}

			width += 1;
			height += 1;

			std::vector<uint16_t> cellHeightData(width * height);

			// Split the data up and update the height fields
			for (int32_t i = 0; i < 3; i++) {
				for (int32_t j = 0; j < 3; j++) {

					auto cell = cells[i * 3 + j];

					if (cell == nullptr)
						continue;

					for (int32_t k = 0; k < height; k++) {
						for (int32_t l = 0; l < width; l++) {
							x = j * (width - 1) + l;
							y = i * (height - 1) + k;
							// We don't want to update the last heights of the right and bottom cells
							if (x >= (width - 1) * 3 || y >= (height - 1) * 3)
								continue;

							int32_t dataOffset = y * 3 * (width - 1) + x;
							int32_t cellOffset = k * width + l;

							// Might fail because the outer right and bottom cells (globally) always have one row less
							// Needs some fix with the terrain generation. We need to make sure that all terrain tiles
							// have the same size.
							cell->heightData[cellOffset] = data[dataOffset];

						}
					}

					for (int32_t k = 0; k < cellHeightData.size(); k++) {
						cellHeightData[k] = (uint16_t)cell->heightData[k];
					}

					cell->heightField->SetData(cellHeightData);

				}

			}

		}

		void TerrainTool::SmoothHeight(Terrain::Terrain *terrain, int32_t size, int32_t contributingRadius,
									   float strength, vec2 position) {

			int32_t LoD = terrain->LoDCount - 1;

			// Get the storage cells
			auto middleMiddle = terrain->GetStorageCell(position.x, position.y, LoD);

			if (middleMiddle == nullptr)
				return;

			auto upperLeft = terrain->storage->GetCell(middleMiddle->x - 1, middleMiddle-> y - 1, LoD);
			auto upperMiddle = terrain->storage->GetCell(middleMiddle->x, middleMiddle->y - 1, LoD);
			auto upperRight = terrain->storage->GetCell(middleMiddle->x + 1, middleMiddle->y - 1, LoD);
			auto middleLeft = terrain->storage->GetCell(middleMiddle->x - 1, middleMiddle->y, LoD);
			auto middleRight = terrain->storage->GetCell(middleMiddle->x + 1, middleMiddle->y, LoD);
			auto bottomLeft = terrain->storage->GetCell(middleMiddle->x - 1, middleMiddle->y + 1, LoD);
			auto bottomMiddle = terrain->storage->GetCell(middleMiddle->x, middleMiddle->y + 1, LoD);
			auto bottomRight = terrain->storage->GetCell(middleMiddle->x + 1, middleMiddle->y + 1, LoD);

			Terrain::TerrainStorageCell* cells[] = {upperLeft, upperMiddle, upperRight,
										   middleLeft, middleMiddle, middleRight,
										   bottomLeft, bottomMiddle, bottomRight};

			// Now bring all height data into one array (we assume that all tiles have the same size)
			int32_t width = middleMiddle->heightField->width - 1;
			int32_t height = middleMiddle->heightField->height - 1;

			std::vector<float> heights(width * height * 9);

			std::fill(heights.begin(), heights.end(), -1.0f);

			auto data = heights.data();

			for (int32_t i = 0; i < 3; i++) {
				for (int32_t j = 0; j < 3; j++) {

					auto cell = cells[i * 3 + j];

					if (cell == nullptr)
						continue;

					for (int32_t k = 0; k < height + 1; k++) {
						for (int32_t l = 0; l < width + 1; l++) {
							int32_t x = j * width + l;
							int32_t y = i * height + k;
							// We don't want to update the last heights of the right and bottom cells
							if (x >= width * 3 || y >= height * 3)
								continue;

							int32_t dataOffset = y * 3 * width + x;
							int32_t cellOffset = k * (width + 1) + l;

							data[dataOffset] = cell->heightData[cellOffset];

						}
					}

				}
			}

			// Apply the kernel on the whole data
			position -= middleMiddle->position;

			int32_t x = (int32_t)floorf(position.x / terrain->resolution);
			int32_t y = (int32_t)floorf(position.y / terrain->resolution);

			x += width;
			y += height;

			std::vector<float> heightsCopy(heights);

			int32_t sizeRadius = (size - 1) / 2;

			for (int32_t i = -sizeRadius; i <= sizeRadius; i++) {
				for (int32_t j = -sizeRadius; j <= sizeRadius; j++) {
					float sum = 0.0f;
					float contributing = 0.0f;
					for (int32_t k = -contributingRadius; k <= contributingRadius; k++) {
						for (int32_t l = -contributingRadius; l <= contributingRadius; l++) {
							if (k == 0 && l == 0)
								continue;
							int32_t xTranslated = x + i + k;
							int32_t yTranslated = y + j + l;
							int32_t index = yTranslated * width * 3 + xTranslated;

							// When we're sampling at the edge of the terrain all
							// values need to be discarded
							if (heightsCopy[index] < 0.0f)
								continue;
							sum += heightsCopy[index];
							contributing += 1.0f;
						}
					}

					sum /= contributing;

					int32_t xTranslated = x + i;
					int32_t yTranslated = y + j;
					int32_t index = yTranslated * width * 3 + xTranslated;
					data[index] = heightsCopy[index] - (heightsCopy[index] - sum) * strength;
				}
			}

			width += 1;
			height += 1;

			std::vector<uint16_t> cellHeightData(width * height);

			// Split the data up and update the height fields
			for (int32_t i = 0; i < 3; i++) {
				for (int32_t j = 0; j < 3; j++) {

					auto cell = cells[i * 3 + j];

					if (cell == nullptr)
						continue;

					for (int32_t k = 0; k < height; k++) {
						for (int32_t l = 0; l < width; l++) {
							x = j * (width - 1) + l;
							y = i * (height - 1) + k;
							// We don't want to update the last heights of the right and bottom cells
							if (x >= (width - 1) * 3 || y >= (height - 1) * 3)
								continue;

							int32_t dataOffset = y * 3 * (width - 1) + x;
							int32_t cellOffset = k * width + l;

							// Might fail because the outer right and bottom cells (globally) always have one row less
							// Needs some fix with the terrain generation. We need to make sure that all terrain tiles
							// have the same size.
							cell->heightData[cellOffset] = data[dataOffset];

						}
					}

					for (int32_t k = 0; k < cellHeightData.size(); k++) {
						cellHeightData[k] = (uint16_t)cell->heightData[k];
					}

					cell->heightField->SetData(cellHeightData);

				}
			}

		}

		void TerrainTool::GenerateNormalData(std::vector<uint16_t>& heightData, std::vector<uint8_t>& normalData, int32_t width, int32_t height, float strength) {

			int32_t dataWidth = (int32_t)sqrtf((float)heightData.size());

			for (int32_t x = 0; x < dataWidth; x++) {
				for (int32_t y = 0; y < dataWidth; y++) {

					float h0 = GetHeight(heightData, dataWidth, x - 1, y - 1, width, height);
					float h1 = GetHeight(heightData, dataWidth, x, y - 1, width, height);
					float h2 = GetHeight(heightData, dataWidth, x + 1, y - 1, width, height);
					float h3 = GetHeight(heightData, dataWidth, x - 1, y, width, height);
					float h4 = GetHeight(heightData, dataWidth, x + 1, y, width, height);
					float h5 = GetHeight(heightData, dataWidth, x - 1, y + 1, width, height);
					float h6 = GetHeight(heightData, dataWidth, x, y + 1, width, height);
					float h7 = GetHeight(heightData, dataWidth, x + 1, y + 1, width, height);

					// Sobel filter
					vec3 normal;

					normal.z = 1.0f / strength;
					normal.y = h0 + 2.0f * h1 + h2 - h5 - 2.0f * h6 - h7;
					normal.x = h0 + 2.0f * h3 + h5 - h2 - 2.0f * h4 - h7;

					normal = (0.5f * glm::normalize(-normal) + 0.5f) * 255.0f;

					normalData[3 * y * dataWidth + 3 * x] = (uint8_t)normal.x;
					normalData[3 * y * dataWidth + 3 * x + 1] = (uint8_t)normal.y;
					normalData[3 * y * dataWidth + 3 * x + 2] = (uint8_t)normal.z;

				}
			}

		}

		float TerrainTool::GetHeight(std::vector<uint16_t>& heightData, int32_t dataWidth,
									 int32_t x, int32_t y, int32_t width, int32_t height) {

			x = x < 0 ? 0 : x;
			x = x >= width ? width - 1 : x;
			y = y < 0 ? 0 : y;
			y = y >= height ? height - 1 : y;

			return (float)heightData[y * dataWidth + x] / 65535.0f;

		}

	}

}