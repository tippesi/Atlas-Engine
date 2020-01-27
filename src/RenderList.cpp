#include <actor/MeshActor.h>
#include "RenderList.h"

#include "libraries/glm/gtx/norm.hpp"

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

		actorBatch->Add(actor);

		actorBatches[actor->mesh] = actorBatch;
		actorBatchBuffers[actor->mesh] = nullptr;
		impostorBuffers[actor->mesh] = nullptr;

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

	void RenderList::UpdateBuffers(Camera* camera) {		

		auto cameraLocation = camera->GetLocation();

		for (auto& actorBatchKey : actorBatches) {
			auto mesh = actorBatchKey.first;
			auto actorBatch = actorBatchKey.second;
			auto hasImpostor = mesh->impostor != nullptr;

			if (!actorBatch->GetSize())
				continue;

			std::vector<mat4> actorMatrices;
			std::vector<mat4> impostorMatrices;

			auto sqdDistance = powf(mesh->impostorDistance, 2.0f);

			for (auto actor : actorBatch->actors) {
				auto distance = glm::distance2(
					vec3(actor->transformedMatrix[3]),
					cameraLocation);

				if (distance < sqdDistance || !hasImpostor) {
					actorMatrices.push_back(actor->transformedMatrix);
				}
				else {
					impostorMatrices.push_back(actor->transformedMatrix);
				}
			}

			if (actorMatrices.size()) {
				auto actorBatchBuffer = new Buffer::VertexBuffer(AE_FLOAT, 16,
					sizeof(mat4), actorMatrices.size(), actorMatrices.data());
				actorBatch->GetObject()->vertexArray.AddInstancedComponent(4,
					actorBatchBuffer);
				actorBatchBuffers[mesh] = actorBatchBuffer;
			}

			if (impostorMatrices.size()) {
				auto impostorBuffer = new Buffer::VertexBuffer(AE_FLOAT, 16,
					sizeof(mat4), impostorMatrices.size(), impostorMatrices.data());
				impostorBuffers[mesh] = impostorBuffer;
			}

		}

	}

	void RenderList::Clear() {

		for (auto& key : actorBatches) {
			if (actorBatchBuffers[key.first])
				key.first->vertexArray.RemoveInstanceComponent(4);
			delete key.second;
		}

		for (auto& key : actorBatchBuffers) {
			delete key.second;
		}

		for (auto& key : impostorBuffers) {
			delete key.second;
		}

		actorBatches.clear();
		actorBatchBuffers.clear();
		impostorBuffers.clear();
		orderedRenderBatches.clear();

	}

}