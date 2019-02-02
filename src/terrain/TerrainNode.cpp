#include "TerrainNode.h"

#include <limits>

TerrainNode::TerrainNode(vec2 location, float resolution, float height, float sideLength, int32_t LoD, int32_t LoDCount,
	ivec2 parentIndex, ivec2 relativeIndex, TerrainStorage* storage, TerrainStorageCell* cell) :
	location(location), sideLength(sideLength), cell(cell), index(relativeIndex), LoD(LoD),
	LoDCount(LoDCount), resolution(resolution), height(height), storage(storage) {

	absoluteIndex = parentIndex * 2 + relativeIndex;

	cell->position = location;

}

TerrainNode::~TerrainNode() {

	// Let the user decide what to do with unused cells
	storage->unusedCells.push_back(cell);

	DeleteChildren();

}

void TerrainNode::Update(Camera* camera, std::vector<TerrainNode*>& renderList, float* LoDDistances) {

	float nodeY = 0.0f;

	if (camera->location.y > height) {
		nodeY = height;
	}
	else {
		if (camera->location.y < 0.0f) {
			nodeY = 0.0f;
		}
		else {
			nodeY = camera->location.y;
		}
	}

	// We should check every corner and the middle of the node to get the minimal distance
	float minDistance = glm::distance(camera->location, vec3(location.x + sideLength / 2.0f, nodeY, location.y + sideLength / 2.0f));

	for (int32_t x = 0; x < 2; x++) {
		for (int32_t y = 0; y < 2; y++) {
			minDistance = glm::min(minDistance, glm::distance(camera->location,
				vec3(location.x + (float)x * sideLength, nodeY, location.y + (float)y * sideLength)));
		}
	}

	if (children.size() > 0) {
		if (LoDDistances[LoD] <= minDistance) {
			DeleteChildren();
		}
		else {
			UpdateChildren(camera, renderList, LoDDistances);
		}
	}
	else {
		if (LoDCount - 1 > LoD) {
			if (LoDDistances[LoD] > minDistance) {
				CreateChildren();
				UpdateChildren(camera, renderList, LoDDistances);
			}
		}
	}

	// We just want to render leafs
	if (children.size() == 0) {
		if (!cell->IsLoaded()) {
			return;
		}
		renderList.push_back(this);
	}

}

void TerrainNode::CreateChildren() {

	// Check if the cells of the children are loaded
	TerrainStorageCell* childrenCells[2][2];
	
	bool creatable = true;

	for (int32_t i = 0; i < 2; i++) {
		for (int32_t j = 0; j < 2; j++) {
			auto childAbsoluteIndex = absoluteIndex * 2 + ivec2(i, j);
			childrenCells[i][j] = storage->GetCell((int32_t)childAbsoluteIndex.x, (int32_t)childAbsoluteIndex.y, LoD + 1);
			if (!childrenCells[i][j]->IsLoaded()) {
				storage->requestedCells.push_back(childrenCells[i][j]);
				creatable = false;
			}
		}
	}

	if (!creatable) {
		return;
	}

	for (int32_t i = 0; i < 2; i++) {
		for (int32_t j = 0; j < 2; j++) {
			children.push_back(TerrainNode(vec2(location.x + (float)i * sideLength / 2.0f * resolution, location.y + (float)j * sideLength / 2.0f * resolution), resolution, height,
				sideLength / 2.0f, LoD + 1, this->LoDCount, absoluteIndex, vec2(i, j), storage, childrenCells[i][j]));
		}
	}

}

void TerrainNode::UpdateChildren(Camera* camera, std::vector<TerrainNode*>& renderList, float* LoDDistances) {

	for (TerrainNode& node : children) {
		node.Update(camera, renderList, LoDDistances);
	}

}

void TerrainNode::DeleteChildren() {

	children.clear();

}