#include "RenderList.h"

namespace Atlas {

	RenderList::RenderList(int32_t type, int32_t mobility) : type(type), mobility(mobility) {


	}

	void RenderList::Add(Mesh::MeshActor *actor) {

		auto actorBatchKey = actorBatches.find(actor->mesh);

		if (actorBatchKey != actorBatches.end()) {
			actorBatchKey->second->Add(actor);
		} else {

			// Create new actorbatch
			auto meshActorBatch = new Mesh::MeshActorBatch(actor->mesh);
			meshActorBatch->Add(actor);

			actorBatches[actor->mesh] = meshActorBatch;

			// Build up all render list batches
			std::map<int32_t, RenderListBatch> renderListBatches;

			for (auto &subData : actor->mesh->data->subData) {

				Shader::ShaderConfig *shaderConfig;

				if (type == AE_GEOMETRY_RENDERLIST) {
					shaderConfig = &actor->mesh->data->materials[subData->materialIndex]->geometryConfig;
				} else {
					shaderConfig = &actor->mesh->data->materials[subData->materialIndex]->shadowConfig;
				}

				auto batchKey = renderListBatches.find(shaderConfig->configBatchID);

				if (batchKey != renderListBatches.end()) {
					batchKey->second.subData.push_back(subData);
				} else {
					RenderListBatch batch;
					batch.meshActorBatch = meshActorBatch;
					batch.subData.push_back(subData);
					renderListBatches[shaderConfig->configBatchID] = batch;
				}

			}

			// Integrate the render list batches into the ordered render batches
			for (auto &renderListBatchKey : renderListBatches) {
				orderedRenderBatches[renderListBatchKey.first].push_back(renderListBatchKey.second);
			}

		}

	}

	void RenderList::Add(Lighting::Light *light) {

		lights.push_back(light);

	}

	void RenderList::Clear() {

		for (auto &actorBatchKey : actorBatches) {
			actorBatchKey.second->Clear();
		}

		lights.clear();

	}

}