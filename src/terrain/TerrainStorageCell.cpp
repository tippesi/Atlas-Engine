#include "TerrainStorageCell.h"

TerrainStorageCell::TerrainStorageCell() {

	x = 0;
	y = 0;
	LoD = 0;

	heightField = nullptr;

}

bool TerrainStorageCell::IsLoaded() {

	if (heightField == nullptr)
		return false;

	return true;

}