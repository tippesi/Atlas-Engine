#pragma once

#include "../System.h"
#include "mesh/Mesh.h"
#include "actor/VegetationActor.h"

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

            void UpdateActorData(Mesh::Mesh* mesh);

            std::vector<ResourceHandle<Mesh::Mesh>> GetMeshes();

            Buffers* GetBuffers(const ResourceHandle<Mesh::Mesh>& mesh);

        private:
            std::map<Mesh::Mesh*, std::vector<Actor::VegetationActor*>> meshToActorMap;
            std::map<Mesh::Mesh*, Buffers> meshToBufferMap;

            std::vector<ResourceHandle<Mesh::Mesh>> meshes;

        };

    }

}