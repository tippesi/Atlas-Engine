#ifndef TERRAINTOOL_H
#define TERRAINTOOL_H

#include "../System.h"

class TerrainTool {

public:
	static void GenerateHeightfieldLoDs(const char* heightfieldFilename, int32_t rootNodeCount, int32_t LoDCount, int32_t patchSize);

};


#endif