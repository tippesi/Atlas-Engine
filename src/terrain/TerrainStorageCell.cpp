#include "TerrainStorageCell.h"

TerrainStorageCell::TerrainStorageCell() {

	x = 0;
	y = 0;
	LoD = 0;

	heightField = nullptr;
	normalMap = nullptr;
	diffuseMap = nullptr;
}

bool TerrainStorageCell::IsLoaded() {

	if (heightField == nullptr)
		return false;

	if (normalMap == nullptr)
		return false;

	if (diffuseMap == nullptr)
		return false;

	if (displacementMap == nullptr)
		return false;

	return true;

}