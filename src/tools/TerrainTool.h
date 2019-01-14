#ifndef TERRAINTOOL_H
#define TERRAINTOOL_H

#include "../System.h"
#include "../terrain/Terrain.h"
#include "../loader/ImageLoader.h"
#include "../Kernel.h"

class TerrainTool {

public:
	static Terrain* GenerateTerrain(Image& heightImage, int32_t rootNodeCount, int32_t LoDCount, int32_t patchSize);

	static void SaveTerrain(Terrain* terrain);

	static void BakeTerrain(Terrain* terrain);

	static void BrushTerrain(Terrain* terrain, Kernel* kernel, float strength, vec2 position);

private:
	static void GenerateNormalData(uint8_t* heightData, uint8_t* normalData, int32_t width, int32_t height, float strength);

	static float GetHeight(uint8_t* heightData, int32_t x, int32_t y, int32_t width, int32_t height);

};


#endif