#ifndef AE_RENDERLIST_H
#define AE_RENDERLIST_H

#include "System.h"
#include "actor/ActorBatch.h"
#include "actor/MeshActor.h"
#include "actor/DecalActor.h"
#include "lighting/Light.h"

#include <map>

#define AE_OPAQUE_RENDERLIST 0
#define AE_SHADOW_RENDERLIST  1

namespace Atlas {

	typedef struct RenderListBatch {

		Actor::ActorBatch<Mesh::Mesh*, Actor::MeshActor*>* meshActorBatch;
		std::vector<Mesh::MeshSubData *> subData;

	} RenderListBatch;


	class RenderList {

	public:
		RenderList(int32_t type = AE_OPAQUE_RENDERLIST);

		void Add(Actor::MeshActor *actor);

		void Add(Actor::DecalActor* actor);

		void Add(Lighting::Light* light);

		void RemoveMesh(Mesh::Mesh *mesh);

		void Clear();

		std::map<Mesh::Mesh*, Actor::ActorBatch<Mesh::Mesh*, Actor::MeshActor*>*> actorBatches;
		std::map<int32_t, std::vector<RenderListBatch>> orderedRenderBatches;

		std::vector<Actor::DecalActor*> decalActors;
		std::vector<Lighting::Light*> lights;

	private:
		int32_t type;

	};

}

#endif