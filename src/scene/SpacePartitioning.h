#ifndef AE_SPACEPARTITIONING_H
#define AE_SPACEPARTITIONING_H

#include "../System.h"
#include "../RenderList.h"
#include "../common/Octree.h"

#include "../actor/StaticMeshActor.h"
#include "../actor/MovableMeshActor.h"
#include "../actor/DecalActor.h"
#include "../actor/AudioActor.h"

namespace Atlas {

	namespace Scene {

		class SpacePartitioning {

		public:
			SpacePartitioning(vec3 min, vec3 max, int32_t depth);

			void Add(Actor::MovableMeshActor* actor);

			void Remove(Actor::MovableMeshActor* actor);

			void Add(Actor::StaticMeshActor* actor);

			void Remove(Actor::StaticMeshActor* actor);

			void Add(Actor::DecalActor* actor);

			void Remove(Actor::DecalActor* actor);

			void Add(Actor::AudioActor* actor);			

			void Remove(Actor::AudioActor* actor);

			void Add(Lighting::Light* light);

			void Remove(Lighting::Light* light);

			void GetRenderList(Common::Frustum frustum, Common::AABB aabb, RenderList& renderList);

			std::vector<Actor::MeshActor*> GetMeshActors();

			std::vector<Actor::StaticMeshActor*> GetStaticMeshActors(Common::AABB aabb);

			std::vector<Actor::MovableMeshActor*> GetMovableMeshActors(Common::AABB aabb);

			std::vector<Actor::DecalActor*> GetDecalActors(Common::AABB aabb);

			std::vector<Actor::AudioActor*> GetAudioActors(Common::AABB aabb);

			std::vector<Lighting::Light*> GetLights();

			void Clear();

		private:
			Common::AABB aabb;
			
			Common::Octree<Actor::MovableMeshActor*> movableMeshOctree;
			Common::Octree<Actor::StaticMeshActor*> staticMeshOctree;
			Common::Octree<Actor::DecalActor*> decalOctree;
			Common::Octree<Actor::AudioActor*> audioOctree;

			std::vector<Lighting::Light*> lights;

		};

	}

}


#endif