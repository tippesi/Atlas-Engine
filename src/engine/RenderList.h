#ifndef AE_RENDERLIST_H
#define AE_RENDERLIST_H

#include "System.h"
#include "actor/ActorBatch.h"
#include "actor/MeshActor.h"
#include "actor/DecalActor.h"
#include "lighting/Light.h"

#include <map>

namespace Atlas {

	struct RenderListBatch {

		Actor::ActorBatch<Mesh::Mesh*, Actor::MeshActor*>* actorBatch;
		std::vector<Mesh::MeshSubData*> subData;

	};

	class RenderList {

	public:
		RenderList(int32_t type = 0/*AE_OPAQUE_CONFIG*/);

		void Add(Actor::MeshActor *actor);

		/*
		 * Adds a range of actors of the same mesh
		 */
		void AddRange(std::vector<Actor::MeshActor*>& actors);

		void UpdateBuffers(Camera* camera);

		void Clear();

		struct Buffers {
			OldBuffer::Buffer* currentMatrices = nullptr;
			OldBuffer::Buffer* lastMatrices = nullptr;
		};

		std::map<Mesh::Mesh*, Actor::ActorBatch<Mesh::Mesh*, Actor::MeshActor*>*> actorBatches;
		std::map<Mesh::Mesh*, Buffers> actorBatchBuffers;
		std::map<Mesh::Mesh*, OldBuffer::Buffer*> impostorBuffers;
		std::map<int32_t, std::vector<RenderListBatch>> orderedRenderBatches;

	private:
		int32_t type;

	};

}

#endif