#include "TerrainTool.h"
#include "../loader/AssetLoader.h"
#include "../loader/ImageLoader.h"
#include "../Log.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"
#include "libraries/stb/stb_image_resize.h"

#include <string>
#include <sys/stat.h>

namespace Atlas {

	namespace Tools {

		Terrain::Terrain* TerrainTool::GenerateTerrain(Common::Image16 &heightImage, int32_t rootNodeSideCount, int32_t LoDCount,
											  int32_t patchSize, float resolution, float height, Material* material) {

			// Check if everything is correct
			int32_t maxNodesPerSide = (int32_t)powf(2.0f, (float)LoDCount - 1.0f) * rootNodeSideCount;

			if (heightImage.height != heightImage.width || heightImage.channels != 1) {
				Log::Error("Some input value wasn't following the specifications while generating the terrain");
				return nullptr;
			}

			auto terrain = new Terrain::Terrain(rootNodeSideCount, LoDCount, patchSize, resolution, height);

			terrain->storage->AddMaterial(0, material);

			// Calculate the number of vertices per tile and resize the height data to map 1:1
			int32_t tileResolution = 8 * patchSize;

			int32_t totalResolution = tileResolution * maxNodesPerSide;

			Common::Image16 heightMap(totalResolution, totalResolution, 1);

			if (heightImage.width != totalResolution) {
				stbir_resize_uint16_generic(heightImage.GetData().data(), heightImage.width, heightImage.height,
					heightImage.width * 2, heightMap.GetData().data(), totalResolution, totalResolution, totalResolution * 2,
					1, -1, 0, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
			}
			else {
				heightMap.SetData(heightImage.GetData());
			}

			// We need some room in the bottom and right for overlapping vertices

			tileResolution += 1;
			int32_t tileResolutionSquared = tileResolution * tileResolution;

			std::vector<uint16_t> cellHeightData(tileResolutionSquared);
			std::vector<uint8_t> cellSplatData(tileResolutionSquared);

			for (size_t i = 0; i < cellSplatData.size(); i++)
				cellSplatData[i] = 0;

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
					cell->splatMap = new Texture::Texture2D(tileResolution,
						tileResolution, AE_R8UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);

					// Now copy a tile of the original image
					// We make sure that every tile has the same size
					// We also increased resolution to make sure that
					// adjacent cells connect to each other
					for (int32_t y = 0; y < tileResolution; y++) {
						for (int32_t x = 0; x < tileResolution; x++) {
							int32_t cellOffset = y * tileResolution + x;
							int32_t xImage = i * (tileResolution - 1) + x;
							int32_t yImage = j * (tileResolution - 1) + y;

							auto sample = (uint16_t)heightMap.Sample(xImage, yImage).r;
							
							cell->heightData[cellOffset] = (float)sample / 65535.0f;
							cellHeightData[cellOffset] = sample;

						}
					}

					cell->heightField->SetData(cellHeightData);
					cell->splatMap->SetData(cellSplatData);

				}
			}

			BakeTerrain(terrain);

			return terrain;

		}

		Terrain::Terrain* TerrainTool::GenerateTerrain(Common::Image16& heightImage, Common::Image8& splatImage,
			int32_t rootNodeSideCount, int32_t LoDCount, int32_t patchSize, float resolution,
			float height, std::vector<Material*> materials) {

			// Check if everything is correct
			int32_t maxNodesPerSide = (int32_t)powf(2.0f, (float)LoDCount - 1.0f) * rootNodeSideCount;

			if (heightImage.height != heightImage.width || heightImage.channels != 1) {
				Log::Error("Some input value wasn't following the specifications while generating the terrain");
				return nullptr;
			}

			auto terrain = new Terrain::Terrain(rootNodeSideCount, LoDCount, patchSize, resolution, height);

			int32_t count = 0;
			for (auto material : materials) {
				terrain->storage->AddMaterial(count++, material);
			}

			// Calculate the number of vertices per tile and resize the height data to map 1:1
			int32_t tileResolution = 8 * patchSize;

			int32_t totalResolution = tileResolution * maxNodesPerSide;

			Common::Image16 heightMap(totalResolution, totalResolution, 1);
			Common::Image8 splatMap(totalResolution, totalResolution, 1);

			if (heightImage.width != totalResolution) {
				stbir_resize_uint16_generic(heightImage.GetData().data(), heightImage.width, heightImage.height,
					heightImage.width * 2, heightMap.GetData().data(), totalResolution, totalResolution, totalResolution * 2,
					1, -1, 0, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
				stbir_resize_uint16_generic(heightImage.GetData().data(), heightImage.width, heightImage.height,
					heightImage.width * 2, heightMap.GetData().data(), totalResolution, totalResolution, totalResolution * 2,
					1, -1, 0, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT, STBIR_COLORSPACE_LINEAR, nullptr);
			}
			else {
				heightMap.SetData(heightImage.GetData());
				splatMap.SetData(splatImage.GetData());
			}

			// We need some room in the bottom and right for overlapping vertices

			tileResolution += 1;
			int32_t tileResolutionSquared = tileResolution * tileResolution;

			std::vector<uint16_t> cellHeightData(tileResolutionSquared);
			std::vector<uint8_t> cellSplatData(tileResolutionSquared);

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
					cell->splatMap = new Texture::Texture2D(tileResolution,
						tileResolution, AE_R8UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);

					// Now copy a tile of the original image
					// We make sure that every tile has the same size
					// We also increased resolution to make sure that
					// adjacent cells connect to each other
					for (int32_t y = 0; y < tileResolution; y++) {
						for (int32_t x = 0; x < tileResolution; x++) {
							int32_t cellOffset = y * tileResolution + x;
							int32_t xImage = i * (tileResolution - 1) + x;
							int32_t yImage = j * (tileResolution - 1) + y;

							auto height = (uint16_t)heightMap.Sample(xImage, yImage).r;
							auto splat = (uint8_t)splatMap.Sample(xImage, yImage).r;

							cell->heightData[cellOffset] = (float)height / 65535.0f;
							cellHeightData[cellOffset] = height;
							cellSplatData[cellOffset] = splat;

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
			int32_t tileResolution = 8 * terrain->patchSizeFactor;
			int32_t tileResolutionSquared = tileResolution * tileResolution;
			int32_t tileCount = terrain->storage->GetCellCount(terrain->LoDCount - 1);

			int32_t tileSideCount = (int32_t)sqrtf((float)tileCount);

			auto heightData = std::vector<uint16_t>((tileSideCount * tileResolution + 1)
				* (tileSideCount * tileResolution + 1));
			auto splatData = std::vector<uint8_t>(heightData.size());

			int32_t heightDataResolution = (int32_t)sqrtf((float)heightData.size());			

			// i is in x direction, j in y direction
			for (int32_t i = 0; i < tileSideCount; i++) {
				for (int32_t j = 0; j < tileSideCount; j++) {
					auto cell = terrain->storage->GetCell(i, j, terrain->LoDCount - 1);
					auto cellSplatData = cell->splatMap->GetData();

					// Now copy a tile of the original image
					// We make sure that every tile has the same size
					// We also increased resolution to make sure that
					// adjacent cells connect to each other
					for (int32_t y = 0; y <= tileResolution; y++) {
						for (int32_t x = 0; x <= tileResolution; x++) {
							int32_t cellOffset = y * (tileResolution + 1) + x;
							int32_t imageOffset = (j * tileResolution + y) * heightDataResolution +
												  i * tileResolution + x;
							heightData[imageOffset] = (uint16_t)(cell->heightData[cellOffset] * 65535.0f);
							splatData[imageOffset] = cellSplatData[cellOffset];	
						}
					}
				}
			}

			tileResolution += 1;

			// Calculate the normals
			auto normalData = std::vector<uint8_t>(heightData.size() * 3);

			GenerateNormalData(heightData, normalData, heightDataResolution,
				heightDataResolution, terrain->heightScale);

			// Iterate through all the LoD level and resize images according to the tile size
			std::vector<uint16_t> tileHeightData(tileResolution * tileResolution);
			std::vector<uint8_t> tileSplatData(tileHeightData.size() * 4);

			auto currentResolution = heightDataResolution;

			for (int32_t k = 0; k < terrain->LoDCount; k++) {

				int32_t Lod = terrain->LoDCount - k - 1;

				int32_t downsample = (int32_t)powf(2.0f, (float)k);
				int32_t tileSideCountLod = tileSideCount / downsample;

				std::vector<uint16_t> resizedHeightData(currentResolution * currentResolution);				
				std::vector<uint8_t> resizedSplatData(resizedHeightData.size() * 4);

				auto resizedHeightDataResolution = (int32_t)sqrtf((float)resizedHeightData.size());

				if (k == 0) {
					resizedHeightData = heightData;					
					resizedSplatData = splatData;
				}
				else {
					// Downsample data (we need to do it manually here)

					for (int32_t y = 0; y < currentResolution; y++) {
						for (int32_t x = 0; x < currentResolution; x++) {
							auto offset0 = ivec2(x, y);
							auto offset1 = ivec2(x, y) * downsample;
							auto index0 = offset0.y * currentResolution + offset0.x;
							auto index1 = offset1.y * heightDataResolution + offset1.x;
							resizedHeightData[index0] = heightData[index1];
							resizedSplatData[index0] = splatData[index1];
						}
					}

				}

				//Common::Image8 img(resizedHeightDataResolution, resizedHeightDataResolution, 4);
				//img.SetData(resizedSplatData);

				//Loader::ImageLoader::SaveImage(img, "splat" + std::to_string(k) + ".png");

				// We need to keep the normal maps at a higher resolution to make the terrain
				// look realistic enough when having low triangle count in the distance
				auto sizeFactor = glm::min(downsample, terrain->bakeResolution / (tileResolution - 1));
				auto normalDataResolution = (tileResolution - 1) * sizeFactor + 3;

				std::vector<uint8_t> tileNormalData(normalDataResolution * 
					normalDataResolution * 3);

				auto resizedNormalDataResolution = 0;
				Common::Image8 resizedNormalMap;

				downsample /= sizeFactor;

				if (downsample == 1) {
					resizedNormalMap = Common::Image8(heightDataResolution,
						heightDataResolution, 3);
					resizedNormalMap.SetData(normalData);
					resizedNormalDataResolution = heightDataResolution;
				}
				else {
					resizedNormalDataResolution = tileSideCountLod * (normalDataResolution - 3) + 1;
					resizedNormalMap = Common::Image8(resizedNormalDataResolution,
						resizedNormalDataResolution, 3);
					for (int32_t y = 0; y < resizedNormalDataResolution; y++) {
						for (int32_t x = 0; x < resizedNormalDataResolution; x++) {
							auto offset1 = ivec2(x, y) * downsample;
							auto index1 = offset1.y * heightDataResolution + offset1.x;
							resizedNormalMap.SetData(x, y, 0, normalData[index1 * 3]);
							resizedNormalMap.SetData(x, y, 1, normalData[index1 * 3 + 1]);
							resizedNormalMap.SetData(x, y, 2, normalData[index1 * 3 + 2]);
						}
					}
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
								int32_t xImage = i * (tileResolution - 1) + x;
								int32_t yImage = j * (tileResolution - 1) + y;
								int32_t totalResolution = currentResolution;

								auto imageOffset = yImage * totalResolution + xImage;

								tileHeightData[cellOffset] = resizedHeightData[imageOffset];
								tileSplatData[cellOffset] = resizedSplatData[imageOffset];
							}
						}
						
						for (int32_t y = -1; y < normalDataResolution - 1; y++) {
							for (int32_t x = -1; x < normalDataResolution - 1; x++) {

								int32_t cellOffset = (y + 1) * normalDataResolution + x + 1;
								int32_t xImage = i * (normalDataResolution - 3) + x;
								int32_t yImage = j * (normalDataResolution - 3) + y;
								
								auto sample = resizedNormalMap.Sample(xImage, yImage);

								tileNormalData[cellOffset * 3] = (uint8_t)sample.r;
								tileNormalData[cellOffset * 3 + 1] = (uint8_t)sample.g;
								tileNormalData[cellOffset * 3 + 2] = (uint8_t)sample.b;
							}
						}

						if (!cell->heightField) {
							cell->heightField = new Texture::Texture2D(tileResolution,
								tileResolution, AE_R16UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
						}
						if (!cell->normalMap) {
							cell->normalMap = new Texture::Texture2D(normalDataResolution,
								normalDataResolution, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR, true, true);
						}
						if (!cell->splatMap) {
							cell->splatMap = new Texture::Texture2D(tileResolution,
								tileResolution, AE_R8UI, GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
						}

						if (cell->normalMap->width != normalDataResolution ||
							cell->normalMap->height != normalDataResolution) {
							delete cell->normalMap;
							cell->normalMap = new Texture::Texture2D(normalDataResolution,
								normalDataResolution, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR, true, true);
						}
						
						cell->normalMap->SetData(tileNormalData);
						cell->heightField->SetData(tileHeightData);
						cell->splatMap->SetData(tileSplatData);

					}
				}

				currentResolution = (currentResolution - 1) / 2 + 1;

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

			int32_t x = (int32_t)floorf(position.x / terrain->resolution);
			int32_t y = (int32_t)floorf(position.y /terrain->resolution);

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

        void TerrainTool::BrushMaterial(Terrain::Terrain* terrain, vec2 position, float size, int32_t slot) {

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

            std::vector<uint8_t> combinedSplatMap(width * height * 9);

            auto& data = combinedSplatMap;

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

							int32_t dataOffset = y * 3 * width + x;
							int32_t cellOffset = k * (width + 1) + l;

							data[dataOffset] = splatData[cellOffset];                              
                            

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

			auto offset = (size - 1) / 2;

            for (uint32_t i = 0; i < size; i++) {
                for (uint32_t j = 0; j < size; j++) {
                    auto xTranslated = x - offset + i;
                    auto yTranslated = y - offset + j;
                    auto index = (int32_t)(yTranslated * width * 3 + xTranslated);
                    data[index] = (uint8_t)slot;
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

							int32_t dataOffset = y * 3 * (width - 1) + x;
							int32_t cellOffset = k * width + l;

							// Might fail because the outer right and bottom cells (globally) always have one row less
							// Needs some fix with the terrain generation. We need to make sure that all terrain tiles
							// have the same size.
							splatData[cellOffset] = data[dataOffset];                            

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