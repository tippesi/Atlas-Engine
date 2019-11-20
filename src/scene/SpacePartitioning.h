#ifndef AE_SPACEPARTITIONING_H
#define AE_SPACEPARTITIONING_H

#include "../System.h"
#include "../RenderList.h"
#include "../volume/Octree.h"

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

			void GetRenderList(Volume::Frustum frustum, RenderList& renderList);

			std::vector<Actor::MeshActor*> GetMeshActors();

			std::vector<Actor::StaticMeshActor*> GetStaticMeshActors(Volume::AABB aabb);

			std::vector<Actor::MovableMeshActor*> GetMovableMeshActors(Volume::AABB aabb);

			std::vector<Actor::DecalActor*> GetDecalActors(Volume::AABB aabb);

			std::vector<Actor::AudioActor*> GetAudioActors(Volume::AABB aabb);

			std::vector<Lighting::Light*> GetLights();

			void Clear();

		private:
			Volume::AABB aabb;
			
			Volume::Octree<Actor::MovableMeshActor*> movableMeshOctree;
			Volume::Octree<Actor::StaticMeshActor*> staticMeshOctree;
			Volume::Octree<Actor::DecalActor*> decalOctree;
			Volume::Octree<Actor::AudioActor*> audioOctree;

			std::vector<Lighting::Light*> lights;

		};

	}

}


#endif