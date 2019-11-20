#include <actor/MeshActor.h>
#include "RenderList.h"

namespace Atlas {

	RenderList::RenderList(int32_t type) : type(type) {


	}

	void RenderList::Add(Actor::MeshActor *actor) {

		auto actorBatchKey = actorBatches.find(actor->mesh);

		if (actorBatchKey != actorBatches.end()) {
			actorBatchKey->second->Add(actor);
			return;
		}

		// Create new actor batch
		auto actorBatch = new Actor::ActorBatch<Mesh::Mesh*, Actor::MeshActor*>(actor->mesh);
		auto actorBatchBuffer = new Buffer::VertexBuffer(AE_FLOAT, 16, sizeof(mat4), 0,
			AE_BUFFER_DYNAMIC_STORAGE);

		actorBatch->Add(actor);

		actorBatches[actor->mesh] = actorBatch;
		actorBatchBuffers[actor->mesh] = actorBatchBuffer;

		// Build up all render list batches
		std::map<int32_t, RenderListBatch> renderListBatches;

		for (auto &subData : actor->mesh->data.subData) {

			auto shaderConfig = actor->mesh->GetConfig(subData.material, type);

			auto batchKey = renderListBatches.find(shaderConfig->shaderID);

			if (batchKey != renderListBatches.end()) {
				batchKey->second.subData.push_back(&subData);
				continue;
			}

			RenderListBatch batch;
			batch.actorBatch = actorBatch;
			batch.subData.push_back(&subData);
			renderListBatches[shaderConfig->shaderID] = batch;

		}

		// Integrate the render list batches into the ordered render batches
		for (auto &renderListBatchKey : renderListBatches) {
			orderedRenderBatches[renderListBatchKey.first].push_back(renderListBatchKey.second);
		}

	}

	void RenderList::UpdateBuffers() {

		std::vector<mat4> actorMatrices;

		for (auto& actorBatchKey : actorBatches) {
			auto actorBatch = actorBatchKey.second;
			auto actorBatchBuffer = actorBatchBuffers[actorBatchKey.first];

			if (!actorBatch->GetSize())
				continue;

			actorBatchBuffer->SetSize(actorBatch->GetSize());

			actorMatrices.reserve(actorBatch->GetSize());

			for (auto actor : actorBatch->actors) {
				actorMatrices.push_back(actor->transformedMatrix);
			}

			actorBatchBuffer->SetData(actorMatrices.data(), 0,
				actorBatch->GetSize());

			actorBatch->GetObject()->vertexArray.AddInstancedComponent(4, actorBatchBuffer);

			actorMatrices.clear();
		}

	}

	void RenderList::Clear() {

		for (auto &actorBatchKey : actorBatches) {
			auto actorBatch = actorBatchKey.second;
			auto actorBatchBuffer = actorBatchBuffers[actorBatchKey.first];

			actorBatch->Clear();
			actorBatchBuffer->SetSize(0);

			actorBatch->GetObject()->vertexArray.DisableComponent(4);
		}

	}

}