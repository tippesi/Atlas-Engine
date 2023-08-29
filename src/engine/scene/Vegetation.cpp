#include "Vegetation.h"

namespace Atlas {

    namespace Scene {

        Vegetation::~Vegetation() {



        }

        void Vegetation::Add(Actor::VegetationActor* actor) {

            auto iterator = meshToActorMap.find(&actor->mesh);
            if (iterator != meshToActorMap.end()) {
                auto& actors = iterator->second;
                actors.push_back(actor);
                return;
            }

            meshToActorMap[&actor->mesh] = std::vector<Actor::VegetationActor*>{ actor };
            meshToBufferMap[&actor->mesh] = Buffers {
                Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4)),
                Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4)),
                Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4))
            };

            meshes.push_back(actor->mesh);

        }

        void Vegetation::UpdateActorData() {

            for (auto& [mesh, value] : meshToActorMap) {
                UpdateActorData(mesh);
            }

        }

        void Vegetation::UpdateActorData(Mesh::Mesh* mesh) {

            struct InstanceData {
                glm::vec4 position;
            };

            auto& actors = meshToActorMap[mesh];
            auto& buffers = meshToBufferMap[mesh];

            std::vector<InstanceData> instanceData;
            for (auto actor : actors) {
                InstanceData instance;

                instance.position = glm::vec4(actor->GetPosition(), 0.0f);

                instanceData.push_back(instance);
            }

            buffers.culledInstanceData.SetSize(instanceData.size());
            buffers.binnedInstanceData.SetSize(instanceData.size());
            buffers.instanceData.SetSize(instanceData.size(), instanceData.data());

        }

        std::vector<ResourceHandle<Mesh::Mesh>> Vegetation::GetMeshes() {

            return meshes;

        }

        Vegetation::Buffers* Vegetation::GetBuffers(const ResourceHandle<Mesh::Mesh>& mesh) {

            return &meshToBufferMap[&mesh];

        }

    }

}