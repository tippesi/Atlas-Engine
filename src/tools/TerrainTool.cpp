#include "TerrainTool.h"
#include "../loader/AssetLoader.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"
#include "libraries/stb/stb_image_resize.h"

#include <string>
#include <sys/stat.h>

namespace Atlas {

	namespace Tools {

		Terrain::Terrain* TerrainTool::GenerateTerrain(Common::Image16 &heightImage, int32_t rootNodeSideCount, int32_t LoDCount,
											  int32_t patchSize, float resolution, float height, Material* material1, Material* material2) {

			// Check if everything is correct
			int32_t maxNodesPerSide = (int32_t)powf(2.0f, (float)LoDCount - 1.0f) * rootNodeSideCount;

			if (heightImage.height != heightImage.width || heightImage.channels != 1) {
				throw AtlasException("Some input value wasn't following the specifications");
			}

			auto terrain = new Terrain::Terrain(rootNodeSideCount, LoDCount, patchSize, resolution, height);

			// Calculate the number of vertices per tile and resize the height data to map 1:1
			int32_t tileResolution = 8 * patchSize;

			int32_t totalResolution = tileResolution * maxNodesPerSide;

			std::vector<uint16_t> heightData(totalResolution * totalResolution);

			stbir_resize_uint16_generic(heightImage.data.data(), heightImage.width, heightImage.height,
				heightImage.width * 2, heightData.data(), totalResolution, totalResolution, totalResolution * 2,
				1, -1, 0, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);

			// We need some room in the bottom and right for overlapping vertices
			tileResolution += 1;
			int32_t tileResolutionSquared = tileResolution * tileResolution;

			std::vector<uint16_t> cellHeightData(tileResolutionSquared);
			std::vector<uint8_t> cellSplatData(tileResolutionSquared * 4);

			for (size_t i = 0; i < cellSplatData.size(); i++)
				if (i % 4 == 0)
					cellSplatData[i] = 255;

			// i is in x direction, j in y direction
			for (int32_t i = 0; i < maxNodesPerSide; i++) {
				for (int32_t j = 0; j < maxNodesPerSide; j++) {
					auto cell = terrain->storage->GetCell(i, j, LoDCount - 1);

					cell->SetMaterial(material1, 0);
					cell->SetMaterial(material2, 1);

					// Create the data structures for the cell
					cell->heightData.resize(tileResolutionSquared);
					cell->heightField = new Texture::Texture2D(tileResolution, 
						tileResolution, AE_R16UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
					cell->normalMap = new Texture::Texture2D(tileResolution, 
						tileResolution, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
					cell->splatMap = new Texture::Texture2D(tileResolution,
						tileResolution, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

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
							cell->heightData[cellOffset] = (float)heightData[imageOffset] / 65535.0f;
							cellHeightData[cellOffset] = heightData[imageOffset];
						}
					}

					cell->heightField->SetData(cellHeightData);
					cell->splatMap->SetData(cellSplatData);

				}
			}

			BakeTerrain(terrain);

			return terrain;

		}

		void TerrainTool::BakeTerrain(Terrain::Terrain *terrain) {

			// Generate one large heightmap (assumes all tiles have the same size)
			int32_t tileResolution = terrain->storage->GetCell(0, 0, terrain->LoDCount - 1)->heightField->width - 1;
			int32_t tileResolutionSquared = tileResolution * tileResolution;
			int32_t tileCount = terrain->storage->GetCellCount(terrain->LoDCount - 1);

			int32_t tileSideCount = (int32_t)sqrtf((float)tileCount);

			auto heightData = std::vector<uint16_t>(tileCount * tileResolution * tileResolution);
			auto splatData = std::vector<uint8_t>(heightData.size() * 4);

			int32_t heightDataResolution = (int32_t)sqrtf((float)heightData.size());

			Material* material1 = nullptr;
			Material* material2 = nullptr;

			// i is in x direction, j in y direction
			for (int32_t i = 0; i < tileSideCount; i++) {
				for (int32_t j = 0; j < tileSideCount; j++) {
					auto cell = terrain->storage->GetCell(i, j, terrain->LoDCount - 1);
					auto cellSplatData = cell->splatMap->GetData();

					material1 = cell->GetMaterial(0);
					material2 = cell->GetMaterial(1);

					// Now copy a tile of the original image
					// We make sure that every tile has the same size
					// We also increased resolution to make sure that
					// adjacent cells connect to each other
					for (int32_t y = 0; y < tileResolution; y++) {
						for (int32_t x = 0; x < tileResolution; x++) {
							int32_t cellOffset = y * (tileResolution + 1) + x;
							int32_t imageOffset = (j * tileResolution + y) * heightDataResolution +
												  i * tileResolution + x;
							heightData[imageOffset] = (uint16_t)(cell->heightData[cellOffset] * 65535.0f);
							splatData[imageOffset * 4] = cellSplatData[cellOffset * 4];
							splatData[imageOffset * 4 + 1] = cellSplatData[cellOffset * 4 + 1];
							splatData[imageOffset * 4 + 2] = cellSplatData[cellOffset * 4 + 2];
							splatData[imageOffset * 4 + 3] = cellSplatData[cellOffset * 4 + 3];
						}
					}
				}
			}

			tileResolution += 1;

			// Calculate the normals
			auto normalData = std::vector<uint8_t>(heightData.size() * 3);

			GenerateNormalData(heightData, normalData, (tileResolution - 1) * tileSideCount,
							   (tileResolution - 1) * tileSideCount, terrain->heightScale);

			// Iterate through all the LoD level and resize images according to the tile size

			std::vector<uint16_t> tileHeightData(tileResolution * tileResolution);
			std::vector<uint8_t> tileNormalData(tileHeightData.size() * 3);
			std::vector<uint8_t> tileSplatData(tileHeightData.size() * 4);

			for (int32_t k = 0; k < terrain->LoDCount; k++) {

				int32_t Lod = terrain->LoDCount - k - 1;

				int32_t downsample = (int32_t)powf(2.0f, (float)k);
				int32_t tileSideCountLod = tileSideCount / downsample;

				std::vector<uint16_t> resizedHeightData(tileSideCountLod * tileSideCountLod * tileResolutionSquared);
				std::vector<uint8_t> resizedNormalData(resizedHeightData.size() * 3);
				std::vector<uint8_t> resizedSplatData(resizedHeightData.size() * 4);

				int32_t resizedHeightDataResolution = (int32_t)sqrtf((float)resizedHeightData.size());

				if (k == 0) {
					resizedHeightData = heightData;
					resizedNormalData = normalData;
					resizedSplatData = splatData;
				}
				else {
					// Downsample data (we need to do it manually here)

					for (int32_t y = 0; y < resizedHeightDataResolution; y++) {
						for (int32_t x = 0; x < resizedHeightDataResolution; x++) {
							auto offset0 = ivec2(x, y);
							auto offset1 = ivec2(x, y) * downsample;
							auto index0 = offset0.y * resizedHeightDataResolution + offset0.x;
							auto index1 = offset1.y * heightDataResolution + offset1.x;
							resizedHeightData[index0] = heightData[index1];
							resizedNormalData[index0 * 3] = normalData[index1 * 3];
							resizedNormalData[index0 * 3 + 1] = normalData[index1 * 3 + 1];
							resizedNormalData[index0 * 3 + 2] = normalData[index1 * 3 + 2];
						}
					}

					stbir_resize_uint8_generic(splatData.data(), heightDataResolution, heightDataResolution,
						heightDataResolution * 4, resizedSplatData.data(), resizedHeightDataResolution,
						resizedHeightDataResolution, resizedHeightDataResolution * 4, 4, -1, 0,
						STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
				}

				/*
				Common::Image16 image(resizedHeightDataResolution, resizedHeightDataResolution, 1);
				image.data = resizedHeightData;
				image.fileFormat = AE_IMAGE_PGM;

				Loader::ImageLoader::SaveImage16(image, std::string("LOD" + std::to_string(k) + ".pgm").c_str());
				*/
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
								int32_t xImage = i * (tileResolution - 1) + x;
								int32_t yImage = j * (tileResolution - 1) + y;
								int32_t imageOffset = 0;
								int32_t totalResolution = resizedHeightDataResolution;
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
								tileHeightData[cellOffset] = resizedHeightData[imageOffset];
								tileNormalData[cellOffset * 3] = resizedNormalData[imageOffset * 3];
								tileNormalData[cellOffset * 3 + 1] = resizedNormalData[imageOffset * 3 + 1];
								tileNormalData[cellOffset * 3 + 2] = resizedNormalData[imageOffset * 3 + 2];
								tileSplatData[cellOffset * 4] = resizedSplatData[imageOffset * 4];
								tileSplatData[cellOffset * 4 + 1] = resizedSplatData[imageOffset * 4 + 1];
								tileSplatData[cellOffset * 4 + 2] = resizedSplatData[imageOffset * 4 + 2];
								tileSplatData[cellOffset * 4 + 3] = resizedSplatData[imageOffset * 4 + 3];
							}
						}

						if (!cell->heightField) {
							cell->heightField = new Texture::Texture2D(tileResolution,
								tileResolution, AE_R16UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
						}
						if (!cell->normalMap) {
							cell->normalMap = new Texture::Texture2D(tileResolution,
								tileResolution, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
						}
						if (!cell->splatMap) {
							cell->splatMap = new Texture::Texture2D(tileResolution,
								tileResolution, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
						}

						cell->heightField->SetData(tileHeightData);
						cell->normalMap->SetData(tileNormalData);
						cell->splatMap->SetData(tileSplatData);

						cell->SetMaterial(material1, 0);
						cell->SetMaterial(material2, 1);

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

			int32_t x = (int32_t)floorf(position.x / (2.0f * terrain->resolution));
			int32_t y = (int32_t)floorf(position.y / (2.0f * terrain->resolution));

			x += width;
			y += height;

			std::vector<std::vector<float>>* weights = nullptr;
			std::vector<std::vector<ivec2>>* offsets = nullptr;

			kernel->Get(weights, offsets);

			for (uint32_t i = 0; i < weights->size(); i++) {
				for (uint32_t j = 0; j < weights->size(); j++) {
					int32_t xTranslated = x + (*offsets)[i][j].x;
					int32_t yTranslated = y + (*offsets)[i][j].y;
					int32_t index = yTranslated * width * 3 + xTranslated;
					float value = data[index] + scale * (*weights)[i][j] / terrain->heightScale;
					data[index] = glm::clamp(value, 0.0f, 1.0f);
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

					for (uint32_t k = 0; k < cellHeightData.size(); k++) {
						cellHeightData[k] = (uint16_t)(cell->heightData[k] * 65535.0f);
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

			int32_t x = (int32_t)floorf(position.x / (2.0f * terrain->resolution));
			int32_t y = (int32_t)floorf(position.y / (2.0f * terrain->resolution));

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

					for (uint32_t k = 0; k < cellHeightData.size(); k++) {
						cellHeightData[k] = (uint16_t)(cell->heightData[k] * 65535.0f);
					}

					cell->heightField->SetData(cellHeightData);

				}
			}

		}

        void TerrainTool::BrushMaterial(Terrain::Terrain* terrain, Kernel* kernel, float strength,
                vec2 position, int32_t channel) {

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

            std::vector<uint8_t> cellDatas[9];

            // Now bring all height data into one array (we assume that all tiles have the same size)
            int32_t width = middleMiddle->splatMap->width - 1;
            int32_t height = middleMiddle->splatMap->height - 1;

            std::vector<uint8_t> combinedSplatMap(width * height * 9 * 4);

            auto data = combinedSplatMap.data();

            for (int32_t i = 0; i < 3; i++) {
                for (int32_t j = 0; j < 3; j++) {

                    auto cell = cells[i * 3 + j];

                    if (cell == nullptr)
                        continue;

                    cellDatas[i * 3 + j] = cell->splatMap->GetData();
                    auto& splatData = cellDatas[i * 3 + j];

                    for (int32_t k = 0; k < height + 1; k++) {
                        for (int32_t l = 0; l < width + 1; l++) {
                            int32_t x = j * width + l;
                            int32_t y = i * height + k;
                            // We don't want to update the last heights of the right and bottom cells
                            if (x >= width * 3 || y >= height * 3)
                                continue;

                            for (int32_t m = 0; m < 4; m++) {
                                int32_t dataOffset = (y * 3 * width + x) * 4 + m;
                                int32_t cellOffset = (k * (width + 1) + l) * 4 + m;

                                data[dataOffset] = splatData[cellOffset];
                            }

                        }

                    }

                }

            }

            // Apply the kernel on the whole data
            position -= middleMiddle->position;

            int32_t x = (int32_t)floorf(position.x / (2.0f * terrain->resolution));
            int32_t y = (int32_t)floorf(position.y / (2.0f * terrain->resolution));

            x += width;
            y += height;

            std::vector<std::vector<float>>* weights = nullptr;
            std::vector<std::vector<ivec2>>* offsets = nullptr;

            kernel->Get(weights, offsets);

            for (uint32_t i = 0; i < weights->size(); i++) {
                for (uint32_t j = 0; j < weights->size(); j++) {
                    auto xTranslated = x + (*offsets)[i][j].x;
                    auto yTranslated = y + (*offsets)[i][j].y;
                    auto index = (yTranslated * width * 3 + xTranslated) * 4;
                    auto value = glm::clamp((float)(data[index + channel]) / 255.0f
                            + strength * (*weights)[i][j], 0.0f, 1.0f);
                    auto sum = 0.0f;
                    for (int32_t k = 0; k < 4; k++) {
                        if (k == channel)
                            continue;
                        sum += ((float)data[index + k]) / 255.0f;
                    }
                    auto factor = sum ? (1.0f - value) / sum : 1.0f;
                    for (int32_t k = 0; k < 4; k++) {
                        if (k == channel)
                            continue;
                        data[index + k] = (uint8_t)(((float)data[index + k]) * factor);
                    }
                    data[index + channel] = (uint8_t)(glm::clamp(value * 255.0f, 0.0f, 255.0f));
                }
            }

            width += 1;
            height += 1;

            // Split the data up and update the height fields
            for (int32_t i = 0; i < 3; i++) {
                for (int32_t j = 0; j < 3; j++) {

                    auto cell = cells[i * 3 + j];

                    if (cell == nullptr)
                        continue;

                    auto& splatData = cellDatas[i * 3 + j];

                    for (int32_t k = 0; k < height; k++) {
                        for (int32_t l = 0; l < width; l++) {
                            x = j * (width - 1) + l;
                            y = i * (height - 1) + k;

                            // We don't want to update the last heights of the right and bottom cells
                            if (x >= (width - 1) * 3 || y >= (height - 1) * 3)
                                continue;

                            for (int32_t m = 0; m < 4; m++) {
                                int32_t dataOffset = (y * 3 * (width - 1) + x) * 4 + m;
                                int32_t cellOffset = (k * width + l) * 4 + m;

                                // Might fail because the outer right and bottom cells (globally) always have one row less
                                // Needs some fix with the terrain generation. We need to make sure that all terrain tiles
                                // have the same size.
                                splatData[cellOffset] = data[dataOffset];
                            }

                        }
                    }

                    cell->splatMap->SetData(splatData);

                }

            }

        }

		void TerrainTool::GenerateNormalData(std::vector<uint16_t>& heightData, std::vector<uint8_t>& normalData, int32_t width, int32_t height, float scale) {

			int32_t dataWidth = (int32_t)sqrtf((float)heightData.size());

			for (int32_t x = 0; x < dataWidth; x++) {
				for (int32_t y = 0; y < dataWidth; y++) {

					float heightL = GetHeight(heightData, dataWidth, x - 1, y, width, height) * scale;
					float heightR = GetHeight(heightData, dataWidth, x + 1, y, width, height) * scale;
					float heightD = GetHeight(heightData, dataWidth, x, y - 1, width, height) * scale;
					float heightU = GetHeight(heightData, dataWidth, x, y + 1, width, height) * scale;

					auto normal = glm::normalize(glm::vec3(heightL - heightR, 1.0f, 
						heightD - heightU));

					normal = (0.5f * normal + 0.5f) * 255.0f;

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