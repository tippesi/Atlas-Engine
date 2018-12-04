#include "RenderList.h"

RenderList::RenderList(int32_t type, int32_t mobility) : type(type), mobility(mobility) {



}

void RenderList::Add(Actor* actor) {

	auto actorBatchKey = actorBatches.find(actor->mesh);

	if (actorBatchKey != actorBatches.end()) {
		actorBatchKey->second->Add(actor);
	}
	else {

		// Create new actorbatch
		ActorBatch* actorBatch = new ActorBatch(actor->mesh);
		actorBatch->Add(actor);

		// Build up all render list batches
		map<ShaderConfig*, RenderListBatch> renderListBatches;

		for (auto& subData : actor->mesh->data->subData) {

			ShaderConfig* shaderConfig;

			if (type == GEOMETRY_RENDERLIST) {
				shaderConfig = actor->mesh->data->materials[subData->materialIndex]->geometryConfig;
			}
			else {
				shaderConfig = actor->mesh->data->materials[subData->materialIndex]->shadowConfig;
			}

			auto batchKey = renderListBatches.find(shaderConfig);

			if (batchKey != renderListBatches.end()) {
				batchKey->second.subData.push_back(subData);
			}
			else {
				RenderListBatch batch;
				batch.actorBatch = actorBatch;
				batch.subData.push_back(subData);
				renderListBatches[shaderConfig] = batch;
			}

		}

		// Integrate the render list batches into the ordered render batches
		for (auto& renderListBatchKey : renderListBatches) {
			orderedRenderBatches[renderListBatchKey.first->batchID].push_back(renderListBatchKey.second);
		}

	}

}

void RenderList::Add(ILight* light) {

	lights.push_back(light);

}

void RenderList::Clear() {

	for (auto& actorBatchKey : actorBatches) {
		actorBatchKey.second->ClearContent();
	}

	lights.clear();

}