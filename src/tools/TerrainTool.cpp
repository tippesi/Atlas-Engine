#include "TerrainTool.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"
#include "libraries/stb/stb_image_resize.h"

#include <string>
#include <sys/stat.h>

/*
void TerrainTool::GenerateHeightfieldLoDs(string heightfieldFilename, int32_t rootNodeCount, int32_t LoDCount, int32_t patchSize) {

	int32_t width, height, channels;

	string directoryPath(heightfieldFilename);

	size_t directoryPathEnd = directoryPath.find_last_of("/\\");

	if (directoryPath.find_last_of("/\\") != string::npos)
		directoryPath = directoryPath.substr(0, directoryPathEnd + 1);
	else
		directoryPath.clear();

	uint8_t* heightfieldData = stbi_load(heightfieldFilename.c_str(), &width, &height, &channels, 1);

	// Just in case the input was somehow wrong
	int32_t nodesPerSide = (int32_t)floor(sqrtf((float)rootNodeCount));
	rootNodeCount = (int32_t)powf((float)nodesPerSide, 2.0f);

	// sum{k = 0 to LODCount - 1} 4^k = (4^(LODCount) - 1) / 3
	int32_t nodesCount = (int32_t)((powf(4.0f, (float)LoDCount) - 1.0f) / 3.0f) * rootNodeCount;

	// We can just have 2^16 nodes due to 16 bit indexing
	if (nodesCount >= 65536) {
		// We have to adjust the LOD Count
		LoDCount = (int32_t)(logf(3.0f * powf(2.0f, 16.0f) / (float)rootNodeCount + 1.0f) / logf(4.0f));
	}

	float terrainSideLength = (float)nodesPerSide * powf(2, (float)LoDCount - 1.0f) * (float)patchSize * 8.0f;

	if (width != height || width != (int32_t)terrainSideLength) {
		throw EngineException("The dimensions of the image doesn't match the given arguments");
	}

	uint8_t* normalMapData = new uint8_t[width * height * 3];

	GenerateNormalData(heightfieldData, normalMapData, width, height, 8.0f);

	int32_t nodeSize = 8 * patchSize;
	int32_t nodeSizeSquared = (nodeSize + 1) * (nodeSize + 1);

	uint8_t* subHeightField = new uint8_t[nodeSizeSquared];
	uint8_t* subNormalMap = new uint8_t[nodeSizeSquared * 3];

	for (int32_t i = 0; i < LoDCount; i++) {

		string dirPath = directoryPath + "LoD" + to_string(i);

#ifdef _WIN32
		_mkdir(dirPath.c_str());
#else
		mkdir(dirPath.c_str(), S_IROTH | S_IWOTH | S_IXOTH);
#endif

		int32_t nodeSideCount = (int32_t)powf(2.0f, (float)i);
		int32_t newResolution = nodeSize * nodeSideCount;

		uint8_t* resizedHeightField = new uint8_t[newResolution * newResolution * rootNodeCount];
		uint8_t* resizedNormalMap = new uint8_t[newResolution * newResolution * rootNodeCount * 3];

		stbir_resize_uint8(heightfieldData, width, height, width, resizedHeightField,
			newResolution * nodesPerSide, newResolution * nodesPerSide, newResolution * nodesPerSide, 1);

		stbir_resize_uint8(normalMapData, width, height, width * 3, resizedNormalMap,
			newResolution * nodesPerSide, newResolution * nodesPerSide, newResolution * nodesPerSide * 3, 3);

		for (int32_t j = 0; j < nodeSideCount * nodesPerSide; j++) {
			for (int32_t k = 0; k < nodeSideCount * nodesPerSide; k++) {

				int32_t xOffset = (int32_t)((float)j / nodeSideCount * (float)newResolution);
				int32_t yOffset = (int32_t)((float)k / nodeSideCount * (float)newResolution);

				// We need to change this. All terrain tiles should have an equal size
				int32_t xExtension = j != (nodeSideCount * nodesPerSide - 1) ? 1 : 0;
				int32_t yExtension = k != (nodeSideCount * nodesPerSide - 1) ? 1 : 0;

				for (int32_t x = xOffset; x < nodeSize + xOffset + xExtension; x++) {
					for (int32_t y = yOffset; y < nodeSize + yOffset + yExtension; y++) {
						subHeightField[(y - yOffset) * (nodeSize + xExtension) + (x - xOffset)] =
							resizedHeightField[y * newResolution * nodesPerSide + x];
						subNormalMap[(y - yOffset) * 3 * (nodeSize + xExtension) + (x - xOffset) * 3] =
							resizedNormalMap[y * 3 * newResolution * nodesPerSide + x * 3];
						subNormalMap[(y - yOffset) * 3 * (nodeSize + xExtension) + (x - xOffset) * 3 + 1] =
							resizedNormalMap[y * 3 * newResolution * nodesPerSide + x * 3 + 1];
						subNormalMap[(y - yOffset) * 3 * (nodeSize + xExtension) + (x - xOffset) * 3 + 2] =
							resizedNormalMap[y * 3 * newResolution * nodesPerSide + x * 3 + 2];
					}
				}

				string filePath = dirPath + "/height" + to_string(j) + "-" + to_string(k) + ".png";

				stbi_write_png(filePath.c_str(), nodeSize + xExtension, nodeSize + yExtension, 1, subHeightField, (nodeSize + xExtension));

				filePath = dirPath + "/normal" + to_string(j) + "-" + to_string(k) + ".png";

				stbi_write_png(filePath.c_str(), nodeSize + xExtension, nodeSize + yExtension, 3, subNormalMap, (nodeSize + xExtension) * 3);

			}
		}

		delete resizedHeightField;
		delete resizedNormalMap;

	}

	delete subHeightField;
	delete subNormalMap;

}
 */

Terrain* TerrainTool::GenerateTerrain(Image &heightImage, int32_t rootNodeCount, int32_t LoDCount, int32_t patchSize) {

	auto terrain = new Terrain(rootNodeCount, LoDCount, patchSize, 1.0f, 30.0f);

    return terrain;

}

void TerrainTool::SaveTerrain(Terrain *terrain) {



}

void TerrainTool::BakeTerrain(Terrain *terrain) {



}

void TerrainTool::BrushTerrain(Terrain *terrain, Kernel *kernel, float scale, vec2 position) {

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

	TerrainStorageCell* cells[] = {upperLeft, upperMiddle, upperRight,
								   middleLeft, middleMiddle, middleRight,
								   bottomLeft, bottomMiddle, bottomRight};

	// Now bring all height data into one array
	uint32_t width = (uint32_t)middleMiddle->heightField->width - 1;
	uint32_t height = (uint32_t)middleMiddle->heightField->height - 1;

	vector<float> heights;
	heights.resize(width * height * 9);

	auto data = heights.data();

	for (uint32_t i = 0; i < 3; i++) {
		for (uint32_t j = 0; j < height; j++) {
			for (uint32_t k = 0; k < 3; k++) {

				auto cell = cells[i * 3 + k];

				if (cell == nullptr)
					continue;

				int32_t dataOffset = i * 3 * width * height + j * 3 * width + k * width;
				int32_t cellOffset = (width + 1) * j;

				for (uint32_t l = 0; l < width; l++) {

					data[dataOffset + l] = (float)cell->heightData[cellOffset + l] * terrain->heightScale / 255.0f;

				}
			}
		}
	}

	// Apply the kernel on the whole data
	position -= middleMiddle->position;

	int32_t x = (int32_t)floorf(position.x);
	int32_t y = (int32_t)floorf(position.y);

	x += width;
	y += height;

	vector<vector<float>>* weights = nullptr;
	vector<vector<ivec2>>* offsets = nullptr;
	
	kernel->Get(weights, offsets);

	for (uint32_t i = 0; i < weights->size(); i++) {
		for (uint32_t j = 0; j < weights->size(); j++) {
			int32_t xTranslated = x + (*offsets)[i][j].x;
			int32_t yTranslated = y + (*offsets)[i][j].y;
			int32_t index = yTranslated * width * 3 + xTranslated;
			data[index] += scale * (*weights)[i][j];
		}
	}

	width += 1;
	height += 1;

	// Split the data up and update the height fields
	for (uint32_t i = 0; i < 3; i++) {
		for (uint32_t j = 0; j < 3; j++) {

			auto cell = cells[i * 3 + j];

			if (cell == nullptr)
				continue;

			for (uint32_t k = 0; k < height; k++) {
				for (uint32_t l = 0; l < width; l++) {
					x = j * (width - 1) + l;
					y = i * (height - 1) + k;
					// We don't want to update the last heights of the right and bottom cells
					if (x >= (width - 1) * 3 || y >= (height - 1) * 3)
						continue;

					int32_t dataOffset = y * 3 * (width - 1) + x;
					int32_t cellOffset = k * width + l;

					cell->heightData[cellOffset] = (uint8_t)glm::clamp(data[dataOffset] * 255.0f / terrain->heightScale, 0.0f, 255.0f);

				}
			}

			cell->heightField->SetData(cell->heightData);

		}
	}

	EngineLog("Finished");

}

void TerrainTool::GenerateNormalData(uint8_t* heightData, uint8_t* normalData, int32_t width, int32_t height, float strength) {

	for (int32_t x = 0; x < width; x++) {
		for (int32_t y = 0; y < height; y++) {

			float h0 = GetHeight(heightData, x - 1, y - 1, width, height);
			float h1 = GetHeight(heightData, x, y - 1, width, height);
			float h2 = GetHeight(heightData, x + 1, y - 1, width, height);
			float h3 = GetHeight(heightData, x - 1, y, width, height);
			float h4 = GetHeight(heightData, x + 1, y, width, height);
			float h5 = GetHeight(heightData, x - 1, y + 1, width, height);
			float h6 = GetHeight(heightData, x, y + 1, width, height);
			float h7 = GetHeight(heightData, x + 1, y + 1, width, height);

			// Sobel filter
			vec3 normal;

			normal.z = 1.0f / strength;
			normal.y = h0 + 2.0f * h1 + h2 - h5 - 2.0f * h6 - h7;
			normal.x = h0 + 2.0f * h3 + h5 - h2 - 2.0f * h4 - h7;

			normal = (0.5f * glm::normalize(-normal) + 0.5f) * 255.0f;

			normalData[3 * y * width + 3 * x] = (uint8_t)normal.x;
			normalData[3 * y * width + 3 * x + 1] = (uint8_t)normal.y;
			normalData[3 * y * width + 3 * x + 2] = (uint8_t)normal.z;

		}
	}

}

float TerrainTool::GetHeight(uint8_t* heightData, int32_t x, int32_t y, int32_t width, int32_t height) {

	x = x < 0 ? 0 : x;
	x = x >= width ? width - 1 : x;
	y = y < 0 ? 0 : y;
	y = y >= height ? height - 1 : y;

	return (float)heightData[y * width + x] / 255.0f;

}