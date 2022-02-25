#include <actor/MeshActor.h>
#include "RenderList.h"

#include "libraries/glm/gtx/norm.hpp"

namespace Atlas {

	RenderList::RenderList(int32_t type) : type(type) {


	}

	void RenderList::Add(Actor::MeshActor *actor) {

		auto actorBatchIt = actorBatches.find(actor->mesh);

		if (actorBatchIt != actorBatches.end()) {
			actorBatchIt->second->Add(actor);
			return;
		}

		// Create new actor batch
		auto actorBatch = new Actor::ActorBatch<Mesh::Mesh*, Actor::MeshActor*>(actor->mesh);		

		actorBatch->Add(actor);

		actorBatches[actor->mesh] = actorBatch;

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

	void RenderList::AddRange(std::vector<Actor::MeshActor*>& actors) {

		auto mesh = actors[0]->mesh;
		auto actorBatchIt = actorBatches.find(mesh);

		if (actorBatchIt != actorBatches.end()) {
			for (auto actor : actors)
				actorBatchIt->second->Add(actor);
			return;
		}

		// Create new actor batch
		auto actorBatch = new Actor::ActorBatch<Mesh::Mesh*, Actor::MeshActor*>(mesh);

		for (auto actor : actors)
			actorBatch->Add(actor);

		actorBatches[mesh] = actorBatch;

		// Build up all render list batches
		std::map<int32_t, RenderListBatch> renderListBatches;

		for (auto& subData : mesh->data.subData) {

			auto shaderConfig = mesh->GetConfig(subData.material, type);

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
		for (auto& renderListBatchKey : renderListBatches) {
			orderedRenderBatches[renderListBatchKey.first].push_back(renderListBatchKey.second);
		}

	}

	void RenderList::UpdateBuffers(Camera* camera) {

		auto cameraLocation = camera->GetLocation();

		for (auto& [mesh, actorBatch] : actorBatches) {
			auto hasImpostor = mesh->impostor != nullptr;

			if (!actorBatch->GetSize())
				continue;

			if (!mesh->castShadow && type == AE_SHADOW_CONFIG)
				continue;

			std::vector<mat4> currentActorMatrices;
			std::vector<mat4> lastActorMatrices;
			std::vector<mat4> impostorMatrices;

			currentActorMatrices.reserve(actorBatch->GetSize());
			lastActorMatrices.reserve(actorBatch->GetSize());
			impostorMatrices.reserve(actorBatch->GetSize());

			auto typeDistance = type == AE_SHADOW_CONFIG ? 
				mesh->impostorShadowDistance : mesh->impostorDistance;
			auto sqdDistance = typeDistance * typeDistance;

			if (hasImpostor) {
				for (auto actor : actorBatch->actors) {
					auto distance = glm::distance2(
						vec3(actor->globalMatrix[3]),
						cameraLocation);

					if (distance < sqdDistance || !hasImpostor) {
						currentActorMatrices.push_back(actor->globalMatrix);
						lastActorMatrices.push_back(actor->lastGlobalMatrix);
					}
					else {
						impostorMatrices.push_back(actor->globalMatrix);
					}
				}
			}
			else {
				for (auto actor : actorBatch->actors) {
					currentActorMatrices.push_back(actor->globalMatrix);
					lastActorMatrices.push_back(actor->lastGlobalMatrix);
				}
			}
			
			if (currentActorMatrices.size()) {
				ActorBatchBuffer buffers;
				auto it = actorBatchBuffers.find(mesh);
				if (it == actorBatchBuffers.end()) {
					buffers.currentMatrices = new Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
						sizeof(mat4), AE_BUFFER_DYNAMIC_STORAGE, currentActorMatrices.size(),
						currentActorMatrices.data());
					buffers.lastMatrices = new Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
						sizeof(mat4), AE_BUFFER_DYNAMIC_STORAGE, lastActorMatrices.size(),
						lastActorMatrices.data());
					actorBatchBuffers[mesh] = buffers;
				}
				else {
					buffers = it->second;
					buffers.currentMatrices->SetSize(currentActorMatrices.size(),
						currentActorMatrices.data());
					buffers.lastMatrices->SetSize(lastActorMatrices.size(),
						lastActorMatrices.data());
				}
			}

			if (impostorMatrices.size()) {
				Buffer::Buffer* buffer = nullptr;
				auto key = impostorBuffers.find(mesh);
				if (key == impostorBuffers.end()) {
					buffer = new Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
						sizeof(mat4), AE_BUFFER_DYNAMIC_STORAGE, impostorMatrices.size(),
						impostorMatrices.data());
					impostorBuffers[mesh] = buffer;
				}
				else {
					buffer = key->second;
					buffer->SetSize(impostorMatrices.size(), impostorMatrices.data());
				}
			}

		}

	}

	void RenderList::Clear() {

		// Set buffers to size zero if no actors were added to them
		for (auto& [mesh, actorBatch] : actorBatches) {
			auto it0 = actorBatchBuffers.find(mesh);
			if (it0 != actorBatchBuffers.end() && !actorBatch->GetSize()) {
				if (it0->second.currentMatrices && it0->second.currentMatrices->GetElementCount()) {
					it0->second.currentMatrices->SetSize(0);
					it0->second.lastMatrices->SetSize(0);
				}
			}
			auto it1 = impostorBuffers.find(mesh);
			if (it1 != impostorBuffers.end() && !actorBatch->GetSize()) {
				if (it1->second && it1->second->GetElementCount()) {
					it1->second->SetSize(0);
				}
			}
		}

		for (auto& [mesh, actorBatch] : actorBatches) {
			auto buffer = actorBatchBuffers.find(mesh);
			if (!buffer->second.currentMatrices)
				continue;
			delete actorBatch;
		}

		actorBatches.clear();
		orderedRenderBatches.clear();

	}

}