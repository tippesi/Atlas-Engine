#include "TerrainTool.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"
#include "libraries/stb/stb_image_resize.h"

#include <string>
#include <sys/stat.h>

void TerrainTool::GenerateHeightfieldLoDs(const char* heightfieldFilename, int32_t rootNodeCount, int32_t LoDCount, int32_t patchSize) {

	int32_t width, height, channels;

	string directoryPath(heightfieldFilename);

	size_t directoryPathEnd = directoryPath.find_last_of("/\\");

	if (directoryPath.find_last_of("/\\") != string::npos)
		directoryPath = directoryPath.substr(0, directoryPathEnd + 1);
	else
		directoryPath.clear();

	uint8_t* heightfieldData = stbi_load(heightfieldFilename, &width, &height, &channels, 1);

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

	float terrainSideLength = (float)nodesPerSide * powf(2, (float)LoDCount) * (float)patchSize * 8.0f;

	if (width != height || width != (int32_t)terrainSideLength) {
		throw EngineException("The dimensions of the image don't match to the given arguments");
	}

	uint8_t* normalMapData = new uint8_t[width * height * 3];

	GenerateNormalData(heightfieldData, normalMapData, width, height, 300.0f);

	int32_t nodeSize = 8 * patchSize;
	int32_t nodeSizeSquared = nodeSize * nodeSize;

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

				for (int32_t x = xOffset; x < nodeSize + xOffset; x++) {
					for (int32_t y = yOffset; y < nodeSize + yOffset; y++) {
						subHeightField[(x - xOffset) * nodeSize + (y - yOffset)] =
							resizedHeightField[x * newResolution * nodesPerSide + y];
						subNormalMap[(x - xOffset) * 3 * nodeSize + (y - yOffset) * 3] =
							resizedNormalMap[x * 3 * newResolution * nodesPerSide + y * 3];
						subNormalMap[(x - xOffset) * 3 * nodeSize + (y - yOffset) * 3 + 1] =
							resizedNormalMap[x * 3 * newResolution * nodesPerSide + y * 3 + 1];
						subNormalMap[(x - xOffset) * 3 * nodeSize + (y - yOffset) * 3 + 2] =
							resizedNormalMap[x * 3 * newResolution * nodesPerSide + y * 3 + 2];
					}
				}

				string filePath = dirPath + "/height" + to_string(k) + "-" + to_string(j) + ".png";

				stbi_write_png(filePath.c_str(), nodeSize, nodeSize, 1, subHeightField, nodeSize);

				filePath = dirPath + "/normal" + to_string(k) + "-" + to_string(j) + ".png";

				stbi_write_png(filePath.c_str(), nodeSize, nodeSize, 3, subNormalMap, nodeSize * 3);

			}
		}

		delete resizedHeightField;
		delete resizedNormalMap;

	}

	delete subHeightField;
	delete subNormalMap;

}

void TerrainTool::GenerateNormalData(uint8_t* heightData, uint8_t* normalData, int32_t width, int32_t height, float strength) {

	int32_t index = 0;

	for (int32_t x = 0; x < width; x++) {
		for (int32_t y = 0; y < height; y++) {

			float heightL = GetHeight(heightData, x - 1, y, width, height, strength);
			float heightR = GetHeight(heightData, x + 1, y, width, height, strength);
			float heightD = GetHeight(heightData, x, y - 1, width, height, strength);
			float heightU = GetHeight(heightData, x, y + 1, width, height, strength);

			glm::vec3 normal = (0.5f * glm::normalize(glm::vec3(heightL - heightR, 1.0f, heightD - heightU)) + 0.5f) * 255.0f;

			normalData[index++] = (uint8_t)normal.x;
			normalData[index++] = (uint8_t)normal.y;
			normalData[index++] = (uint8_t)normal.z;

		}
	}

}

float TerrainTool::GetHeight(uint8_t* heightData, int32_t x, int32_t y, int32_t width, int32_t height, float strength) {

	x = x < 0 ? 0 : x;
	x = x >= width ? width - 1 : x;
	y = y < 0 ? 0 : y;
	y = y >= height ? height - 1 : y;

	return (float)heightData[x * width + y] / 255.0f * strength;

}