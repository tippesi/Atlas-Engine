#ifndef TERRAINTOOL_H
#define TERRAINTOOL_H

#include "../System.h"

class TerrainTool {

public:
	static void GenerateHeightfieldLoDs(string heightfieldFilename, int32_t rootNodeCount, int32_t LoDCount, int32_t patchSize);

private:
	static void GenerateNormalData(uint8_t* heightData, uint8_t* normalData, int32_t width, int32_t height, float strength);

	static float GetHeight(uint8_t* heightData, int32_t x, int32_t y, int32_t width, int32_t height);

};


#endif