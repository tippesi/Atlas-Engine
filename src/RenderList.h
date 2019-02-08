#ifndef AE_RENDERLIST_H
#define AE_RENDERLIST_H

#include "System.h"
#include "lighting/ILight.h"
#include "mesh/MeshActorBatch.h"

#include <map>

#define AE_GEOMETRY_RENDERLIST 0
#define AE_SHADOW_RENDERLIST  1

namespace Atlas {

	typedef struct RenderListBatch {

		Mesh::MeshActorBatch *meshActorBatch;
		std::vector<Mesh::MeshSubData *> subData;

	} RenderListBatch;


	class RenderList {

	public:
		RenderList(int32_t type, int32_t mobility);

		void Add(Mesh::MeshActor *actor);

		void Add(Lighting::ILight *light);

		void RemoveMesh(Mesh::Mesh *mesh);

		void Clear();

		std::vector<Lighting::ILight *> lights;
		std::map<Mesh::Mesh *, Mesh::MeshActorBatch *> actorBatches;
		std::map<int32_t, std::vector<RenderListBatch>> orderedRenderBatches;

	private:
		int32_t type;
		int32_t mobility;

	};

}

#endif