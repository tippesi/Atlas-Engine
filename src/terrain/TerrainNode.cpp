#include "TerrainNode.h"

TerrainNode::TerrainNode(vec2 location, float resolution, float height, int32_t LoD, int32_t LoDCount,
	vec2 parentIndex, vec2 relativeIndex, TerrainStorage* storage, TerrainStorageCell* cell) : 
	location(location), resolution(resolution), height(height), LoD(LoD), 
	LoDCount(LoDCount), index(relativeIndex), storage(storage), cell(cell) {

	absoluteIndex = parentIndex + relativeIndex;

}

void TerrainNode::Update(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances) {

	vec3 absoluteLocation = vec3(0.0f);

	if (camera->location.y > height) {
		absoluteLocation = vec3(location.x, height, location.y);
	}
	else {
		if (camera->location.y < 0.0f) {
			absoluteLocation = vec3(location.x, 0.0f, location.y);
		}
		else {
			absoluteLocation = vec3(location.x, camera->location.y, location.y);
		}
	}

	float distance = glm::distance(camera->location, absoluteLocation);

	if (children.size() > 0) {
		// Maybe we should use a small offset here, in case somebody is moving
		// between two LoD distances all the time. This would reduce reloading 
		// the terrain nodes and maybe would also reduce the number of cell deletions.
		if (LoDDistances[LoD + 1] > distance) {
			DeleteChildren();
		}
		else {
			UpdateChildren(camera, renderList, LoDDistances);
		}
	}
	else {
		if (LoDCount > LoD) {
			if (LoDDistances[LoD] < distance) {
				CreateChildren();
			}
		}
	}

	// We just want to render leafs
	if (children.size() == 0) {
		renderList.push_back(this);
	}

}

void TerrainNode::CreateChildren() {

	// Check if the cells of the children are loaded
	TerrainStorageCell* childrenCells[2][2];

	bool creatable = true;

	for (int32_t i = 0; i < 2; i++) {
		for (int32_t j = 0; j < 2; j++) {
			vec2 childAbsoluteIndex = absoluteIndex + vec2(i, j);
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
			// children.push_back(new TerrainNode(vec2((float)i * ratio, (float)j * ratio), resolution, height,
				// 0, this->LoDCount, vec2(0, 0), vec2(i, j), storage, cell));
		}
	}

}

void TerrainNode::UpdateChildren(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances) {

	for (TerrainNode*& node : children) {
		node->Update(camera, renderList, LoDDistances);
	}

}

void TerrainNode::DeleteChildren() {

	for (TerrainNode*& node : children) {
		delete node;
	}

}

TerrainNode::~TerrainNode() {

	// Let the user decide what to do with unused cells
	storage->unusedCells.push_back(cell);

}