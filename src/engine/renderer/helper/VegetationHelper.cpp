#include "VegetationHelper.h"

#include "../../pipeline/PipelineManager.h"
#include "../../graphics/Profiler.h"

namespace Atlas {

    namespace Renderer {

        namespace Helper {

            VegetationHelper::VegetationHelper() {

                indirectDrawCallBuffer = Buffer::Buffer(Buffer::BufferUsageBits::IndirectBufferBit,
                    sizeof(DrawElementsIndirectCommand), 0);

                meshInformationBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
                    Buffer::BufferUsageBits::HostAccessBit, sizeof(MeshInformation));

                meshSubdataInformationBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit |
                    Buffer::BufferUsageBits::HostAccessBit, sizeof(MeshSubdataInformation));

                instanceCounterBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                    sizeof(uint32_t), 0);

                binCounterBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                    sizeof(uint32_t), 0);

                binOffsetBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit,
                    sizeof(uint32_t), 0);

            }

            void VegetationHelper::PrepareInstanceBuffer(Scene::Clutter& vegetation,
                const CameraComponent& camera, Graphics::CommandList* commandList) {

                struct PushConstants {
                    int32_t instanceCount;
                    int32_t meshIdx;
                    uint32_t binCount;
                };

                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers;

                Graphics::Profiler::BeginQuery("Culling");

                auto meshes = vegetation.GetMeshes();

                // Check if the vegetation meshes have changed
                size_t meshFoundCount = 0;
                for (auto mesh : meshes) meshFoundCount += meshToIdxMap.find(&mesh) != meshToIdxMap.end() ? 1 : 0;
                if (meshFoundCount != meshToIdxMap.size() || meshFoundCount != meshes.size())
                    GenerateBuffers(vegetation, commandList);

                if (!meshFoundCount && meshToIdxMap.size() == 0) return;

                meshInformationBuffer.Bind(commandList, 3, 0);
                meshSubdataInformationBuffer.Bind(commandList, 3, 1);
                binCounterBuffer.Bind(commandList, 3, 2);
                binOffsetBuffer.Bind(commandList, 3, 3);
                instanceCounterBuffer.Bind(commandList, 3, 4);

                Graphics::Profiler::BeginQuery("Cull and count bins");

                // Cull unseen vegetation and count bins
                {
                    auto pipelineConfig = PipelineConfig("vegetation/instanceCulling.csh");
                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                    commandList->BindPipeline(pipeline);
                
                    for (auto mesh : meshes) {
                        auto buffers = vegetation.GetBuffers(mesh);

                        auto instanceCount = int32_t(buffers->instanceData.GetElementCount());
                        auto idx = meshToIdxMap[&mesh];

                        buffers->instanceData.Bind(commandList, 3, 5);
                        buffers->culledInstanceData.Bind(commandList, 3, 6);

                        PushConstants constants = {
                            .instanceCount = instanceCount,
                            .meshIdx = idx,
                            .binCount = binCount
                        };
                        commandList->PushConstants("constants", &constants);

                        auto groupSize = 64;
                        auto groupCount = instanceCount / groupSize;
                        groupCount += (groupCount % groupSize == 0) ? 0 : 1;

                        commandList->Dispatch(groupCount, 1, 1);
                    }

                    bufferBarriers.clear();
                    bufferBarriers.push_back({ binCounterBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers);
                }

                Graphics::Profiler::EndAndBeginQuery("Compute bin offsets");

                {
                    auto pipelineConfig = PipelineConfig("vegetation/instanceBinningOffset.csh");
                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                    commandList->BindPipeline(pipeline);

                    auto binCount = int32_t(binCounterBuffer.GetElementCount());

                    auto groupSize = 64;
                    auto groupCount = binCount / groupSize;
                    groupCount += (groupCount % groupSize == 0) ? 0 : 1;

                    commandList->Dispatch(groupCount, 1, 1);

                    bufferBarriers.clear();
                    bufferBarriers.push_back({binOffsetBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                    bufferBarriers.push_back({binCounterBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                    bufferBarriers.push_back({instanceCounterBuffer.Get(), VK_ACCESS_SHADER_READ_BIT});
                    for (auto mesh : meshes) {
                        auto buffers = vegetation.GetBuffers(mesh);
                        bufferBarriers.push_back({buffers->culledInstanceData.Get(),
                            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                    }
                    commandList->PipelineBarrier(imageBarriers, bufferBarriers);
                }

                Graphics::Profiler::EndAndBeginQuery("Sort by bins");

                // Move culled per instance data into own buffer
                {
                    auto pipelineConfig = PipelineConfig("vegetation/instanceBinning.csh");
                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                    commandList->BindPipeline(pipeline);

                    for (auto mesh : meshes) {
                        auto buffers = vegetation.GetBuffers(mesh);

                        auto instanceCount = int32_t(buffers->instanceData.GetElementCount());
                        auto idx = meshToIdxMap[&mesh];

                        buffers->culledInstanceData.Bind(commandList, 3, 5);
                        buffers->binnedInstanceData.Bind(commandList, 3, 6);

                        PushConstants constants = {
                            .instanceCount = instanceCount,
                            .meshIdx = idx,
                            .binCount = binCount
                        };
                        commandList->PushConstants("constants", &constants);

                        auto groupSize = 64;
                        auto groupCount = instanceCount / groupSize;
                        groupCount += (groupCount % groupSize == 0) ? 0 : 1;

                        commandList->Dispatch(groupCount, 1, 1);
                    }
                }

                Graphics::Profiler::EndAndBeginQuery("Command buffer update");

                // Prepare the command buffer
                {
                    auto pipelineConfig = PipelineConfig("vegetation/instanceDrawCall.csh");
                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                    commandList->BindPipeline(pipeline);

                    int32_t drawCallCount = int32_t(indirectDrawCallBuffer.GetElementCount());
                    commandList->PushConstants("constants", &drawCallCount);

                    auto groupSize = 64;
                    auto groupCount = glm::max(drawCallCount / groupSize, 1);
                    if (drawCallCount > groupSize)
                        groupCount += groupCount % groupSize == 0 ? 0 : 1;

                    indirectDrawCallBuffer.Bind(commandList, 3, 7);

                    commandList->Dispatch(groupCount, 1, 1);
                }

                // Reset lod counter buffer
                if (binCounterBuffer.GetElementCount()) {
                    ResetCounterBuffers(commandList);
                }

                // Reset access mask manually
                indirectDrawCallBuffer.Get()->accessMask = VK_ACCESS_SHADER_WRITE_BIT;
                // Can't group barriers together because of different access patterns
                commandList->BufferMemoryBarrier(indirectDrawCallBuffer.Get(), VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

                bufferBarriers.clear();
                for (auto mesh : meshes) {
                    auto buffers = vegetation.GetBuffers(mesh);
                    bufferBarriers.push_back({buffers->binnedInstanceData.Get(),
                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT });
                }
                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

                Graphics::Profiler::EndQuery();
                Graphics::Profiler::EndQuery();

            }

            Buffer::Buffer* VegetationHelper::GetCommandBuffer() {
                
                return &indirectDrawCallBuffer;

            }

            size_t VegetationHelper::GetCommandBufferOffset(ResourceHandle<Mesh::Mesh>& mesh,
                Mesh::MeshSubData& subData) {

                auto meshIdx = meshToIdxMap[&mesh];

                for (size_t i = 0; i < meshSubdataInformation.size(); i++) {
                    auto& subdataInfo = meshSubdataInformation[i];

                    if (subdataInfo.meshIdx == meshIdx &&
                        subdataInfo.indicesOffset == subData.indicesOffset &&
                        subdataInfo.indicesCount == subData.indicesCount) {
                        return i * size_t(binCount);
                    }
                }

                return 0;

            }

            void VegetationHelper::GenerateBuffers(Scene::Clutter& vegetation, Graphics::CommandList* commandList) {

                auto meshes = vegetation.GetMeshes();

                // Delete old information
                meshSubdataInformation.clear();

                uint32_t idx = 0;
                uint32_t binCounter = 0;
                uint32_t meshCounter = 0;
                std::vector<MeshInformation> meshInformation;
                for (auto mesh : meshes) {
                    meshInformation.push_back({ vec4(mesh->data.aabb.min, reinterpret_cast<float&>(binCounter)),
                                                vec4(mesh->data.aabb.max, 0.0)});
                    meshToIdxMap[&mesh] = int32_t(idx);

                    for (auto& subdata : mesh->data.subData) {
                        meshSubdataInformation.push_back({ idx,
                            subdata.indicesOffset, subdata.indicesCount });
                    }

                    idx++;
                    binCounter += binCount;
                    meshCounter++;
                }

                // Each bin needs a seperate draw call
                indirectDrawCallBuffer.SetSize(meshSubdataInformation.size() * size_t(binCounter));

                binCounterBuffer.SetSize(size_t(binCounter));
                binOffsetBuffer.SetSize(size_t(binCounter));
                instanceCounterBuffer.SetSize(size_t(meshCounter));

                ResetCounterBuffers(commandList);

                // There should be no need for a barrier after this write, since it happens on the host side
                // and should be available immediately
                meshInformationBuffer.SetSize(meshInformation.size(), 
                    meshInformation.data());
                meshSubdataInformationBuffer.SetSize(meshSubdataInformation.size(),
                    meshSubdataInformation.data());
            }

            void VegetationHelper::ResetCounterBuffers(Graphics::CommandList* commandList) {

                std::vector<Graphics::BufferBarrier> bufferBarriers;
                std::vector<Graphics::ImageBarrier> imageBarriers;

                bufferBarriers.push_back({binCounterBuffer.Get(), VK_ACCESS_TRANSFER_WRITE_BIT});
                bufferBarriers.push_back({instanceCounterBuffer.Get(), VK_ACCESS_TRANSFER_WRITE_BIT});
                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);

                uint32_t zero = 0;
                commandList->FillBuffer(binCounterBuffer.Get(), &zero);
                commandList->FillBuffer(instanceCounterBuffer.Get(), &zero);

                bufferBarriers.clear();
                bufferBarriers.push_back({binCounterBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT});
                bufferBarriers.push_back({instanceCounterBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT});
                commandList->PipelineBarrier(imageBarriers, bufferBarriers, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            }

        }

    }

}