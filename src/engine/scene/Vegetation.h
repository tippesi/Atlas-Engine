#ifndef AE_VEGETATION_H
#define AE_VEGETATION_H

#include "../System.h"
#include "actor/VegetationActor.h"
#include "mesh/VegetationMesh.h"

#include <map>

namespace Atlas {

    namespace Scene {

        class Vegetation {

        public:
            struct Buffers {
                Buffer::Buffer instanceData;
                Buffer::Buffer culledInstanceData;
                Buffer::Buffer binnedInstanceData;
            };

            Vegetation() = default;

            ~Vegetation();

            void Add(Actor::VegetationActor* actor);

            void UpdateActorData();

            void UpdateActorData(Mesh::VegetationMesh* mesh);

            std::vector<Mesh::VegetationMesh*> GetMeshes();

            Buffers GetBuffers(Mesh::VegetationMesh* mesh);

        private:
            std::map<Mesh::VegetationMesh*, std::vector<Actor::VegetationActor*>> meshToActorMap;
            std::map<Mesh::VegetationMesh*, Buffers> meshToBufferMap;

        };

    }

}

#endif