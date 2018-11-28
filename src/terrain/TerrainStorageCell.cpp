#include "TerrainStorageCell.h"

TerrainStorageCell::TerrainStorageCell() {

	x = 0;
	y = 0;
	LoD = 0;

	heightData = nullptr;

	heightField = nullptr;
	normalMap = nullptr;

}

bool TerrainStorageCell::IsLoaded() {

	if (heightField == nullptr)
		return false;

	if (normalMap == nullptr)
		return false;

	return true;

}