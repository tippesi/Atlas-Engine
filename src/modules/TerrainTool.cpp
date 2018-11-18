#include "TerrainTool.h"

#include "libraries/stb/stb_image.h"
#include "libraries/stb/stb_image_write.h"
#include "libraries/stb/stb_image_resize.h"

#include <string>
#include <sys/stat.h>
#include <direct.h>

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

	int32_t nodeSize = 8 * patchSize;
	int32_t nodeSizeSquared = nodeSize * nodeSize;

	uint8_t* subImage = new uint8_t[nodeSizeSquared];

	for (int32_t i = 0; i < LoDCount; i++) {

		string dirPath = directoryPath + "LoD" + to_string(i);

		mkdir(dirPath.c_str());

		int32_t nodeSideCount = (int32_t)powf(2.0f, (float)i);
		int32_t newResolution = nodeSize * nodeSideCount;

		uint8_t* resizedImage = new uint8_t[newResolution * newResolution * rootNodeCount];

		stbir_resize_uint8(heightfieldData, width, height, width, resizedImage, 
			newResolution * nodesPerSide, newResolution * nodesPerSide, newResolution * nodesPerSide, 1);

		for (int32_t j = 0; j < nodeSideCount * nodesPerSide; j++) {
			for (int32_t k = 0; k < nodeSideCount * nodesPerSide; k++) {

				int32_t xOffset = (int32_t)((float)j / nodeSideCount * (float)newResolution);
				int32_t yOffset = (int32_t)((float)k / nodeSideCount * (float)newResolution);

				for (int32_t x = xOffset; x < nodeSize + xOffset; x++) {
					for (int32_t y = yOffset; y < nodeSize + yOffset; y++) {
						subImage[(x - xOffset) * nodeSize + (y - yOffset)] =
							resizedImage[x * newResolution * nodesPerSide + y];
					}
				}

				string filePath = dirPath + "/height" + to_string(k) + "-" + to_string(j) + ".png";

				stbi_write_png(filePath.c_str(), nodeSize, nodeSize, 1, subImage, nodeSize);

			}
		}

		delete resizedImage;

	}

	delete subImage;

}