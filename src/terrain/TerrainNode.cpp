#include "TerrainNode.h"

TerrainNode::TerrainNode(vec2 location, int32_t terrainResolution, int32_t LoD, vec2 index, TerrainStorage* storage) : 
	location(location), terrainResolution(terrainResolution), LoD(LoD), index(index), storage(storage) {

	children = nullptr;

	cell = storage->GetCell()

}

void TerrainNode::Update(Camera* camera, vector<TerrainNode*>& renderList, float* LoDDistances) {

	float distance = glm::distance(vec2(camera->location.x, camera->location.z), location);



}