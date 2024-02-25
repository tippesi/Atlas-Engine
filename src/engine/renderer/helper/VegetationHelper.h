#pragma once

#include "System.h"
#include "scene/Clutter.h"
#include "buffer/Buffer.h"
#include "scene/components/CameraComponent.h"

#include <map>

namespace Atlas {

    namespace Renderer {

        namespace Helper {

            class VegetationHelper {

            public:
                VegetationHelper();

                void PrepareInstanceBuffer(Scene::Clutter& vegetation,
                    const CameraComponent& camera, Graphics::CommandList* commandList);

                Buffer::Buffer* GetCommandBuffer();

                size_t GetCommandBufferOffset(ResourceHandle<Mesh::Mesh>& mesh,
                    Mesh::MeshSubData& subData);

                struct DrawElementsIndirectCommand {
                    uint32_t count;
                    uint32_t instanceCount;
                    uint32_t firstIndex;
                    uint32_t baseVertex;
                    uint32_t baseInstance;
                    uint32_t padding1;
                    uint32_t padding2;
                    uint32_t padding3;
                };

                const uint32_t binCount = 64;

            private:
                struct MeshInformation {
                    vec4 aabbMin;
                    vec4 aabbMax;
                };

                struct MeshSubdataInformation {
                    uint32_t meshIdx;
                    uint32_t indicesOffset;
                    uint32_t indicesCount;
                    uint32_t padding;
                };

                void GenerateBuffers(Scene::Clutter& vegetation, Graphics::CommandList* commandList);

                void ResetCounterBuffers(Graphics::CommandList* commandList);

                Buffer::Buffer indirectDrawCallBuffer;

                Buffer::Buffer instanceCounterBuffer;
                
                Buffer::Buffer binCounterBuffer;
                Buffer::Buffer binOffsetBuffer;

                Buffer::Buffer meshInformationBuffer;
                Buffer::Buffer meshSubdataInformationBuffer;

                std::map<Mesh::Mesh*, int32_t> meshToIdxMap;
                std::vector<MeshSubdataInformation> meshSubdataInformation;

            };

        }

    }

}