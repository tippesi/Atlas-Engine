#include "VegetationHelper.h"
#include "../../graphics/Profiler.h"

namespace Atlas {

    namespace Renderer {

        namespace Helper {

            VegetationHelper::VegetationHelper() {

                /*
                instanceCullingShader.AddStage(AE_COMPUTE_STAGE, "vegetation/instanceCulling.csh");
                instanceCullingShader.Compile();

                instanceBinningShader.AddStage(AE_COMPUTE_STAGE, "vegetation/instanceBinning.csh");
                instanceBinningShader.Compile();

                instanceBinningOffsetShader.AddStage(AE_COMPUTE_STAGE, "vegetation/instanceBinningOffset.csh");
                instanceBinningOffsetShader.Compile();

                instanceDrawCallShader.AddStage(AE_COMPUTE_STAGE, "vegetation/instanceDrawCall.csh");
                instanceDrawCallShader.Compile();

                indirectDrawCallBuffer = Buffer::Buffer(AE_DRAW_INDIRECT_BUFFER, 
                    sizeof(DrawElementsIndirectCommand), 0);

                meshInformationBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
                    sizeof(MeshInformation), 0);

                meshSubdataInformationBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
                    sizeof(MeshSubdataInformation), 0);

                instanceCounterBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
                    sizeof(uint32_t), 0);

                binCounterBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, 
                    sizeof(uint32_t), 0);

                binOffsetBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
                    sizeof(uint32_t), 0);
                 */

            }

            void VegetationHelper::PrepareInstanceBuffer(Scene::Vegetation& vegetation, Camera* camera) {

                /*
                Graphics::Profiler::BeginQuery("Culling");

                auto meshes = vegetation.GetMeshes();

                // Check if the vegetation meshes have changed
                size_t meshFoundCount = 0;
                //for (auto mesh : meshes) meshFoundCount += meshToIdxMap.find(mesh) != meshToIdxMap.end() ? 1 : 0;
                //if (meshFoundCount != meshToIdxMap.size() || meshFoundCount != meshes.size())
                    //GenerateBuffers(vegetation);

                meshInformationBuffer.BindBase(0);
                meshSubdataInformationBuffer.BindBase(1);
                binCounterBuffer.BindBase(2);
                binOffsetBuffer.BindBase(3);
                instanceCounterBuffer.BindBase(4);
                

                Graphics::Profiler::BeginQuery("Cull and count bins");

                // Cull unseen vegetation and count bins
                {
                    instanceCullingShader.Bind();

                    instanceCullingShader.GetUniform("cameraLocation")->SetValue(camera->GetLocation());
                    instanceCullingShader.GetUniform("frustumPlanes")->SetValue(camera->frustum.GetPlanes().data(), 6);
                
                    for (auto mesh : meshes) {
                        auto buffers = vegetation.GetBuffers(mesh);

                        auto instanceCount = int32_t(buffers->instanceData.GetElementCount());
                        auto idx = meshToIdxMap[mesh];

                        buffers->instanceData.BindBase(5);
                        buffers->culledInstanceData.BindBase(6);


                        instanceCullingShader.GetUniform("instanceCount")->SetValue(instanceCount);
                        instanceCullingShader.GetUniform("meshIdx")->SetValue(idx);
                        instanceCullingShader.GetUniform("binCount")->SetValue(binCount);

                        auto groupSize = 64;
                        auto groupCount = instanceCount / groupSize;
                        groupCount += (groupCount % groupSize == 0) ? 0 : 1;

                        // glDispatchCompute(groupCount, 1, 1);
                    }
                }

                Graphics::Profiler::EndAndBeginQuery("Compute bin offsets");

                {
                    instanceBinningOffsetShader.Bind();

                    auto binCount = int32_t(binCounterBuffer.GetElementCount());

                    auto groupSize = 64;
                    auto groupCount = binCount / groupSize;
                    groupCount += (groupCount % groupSize == 0) ? 0 : 1;

                    // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                    // glDispatchCompute(groupCount, 1, 1);
                }

                Graphics::Profiler::EndAndBeginQuery("Sort by bins");

                // Move culled per instance data into own buffer
                {
                    instanceBinningShader.Bind();
                    bool firstMesh = true;

                    instanceBinningShader.GetUniform("cameraLocation")->SetValue(camera->GetLocation());

                    for (auto mesh : meshes) {
                        auto buffers = vegetation.GetBuffers(mesh);

                        auto instanceCount = int32_t(buffers->instanceData.GetElementCount());
                        auto idx = meshToIdxMap[mesh];

                        buffers->culledInstanceData.BindBase(5);
                        buffers->binnedInstanceData.BindBase(6);
                        
                        instanceBinningShader.GetUniform("instanceCount")->SetValue(instanceCount);
                        instanceBinningShader.GetUniform("meshIdx")->SetValue(idx);
                        instanceBinningShader.GetUniform("binCount")->SetValue(binCount);

                        auto groupSize = 64;
                        auto groupCount = instanceCount / groupSize;
                        groupCount += (groupCount % groupSize == 0) ? 0 : 1;

                        // Just use a memory barrier for the first call
                        // It's synchronized afterwards
                        if (firstMesh) {
                            firstMesh = false;
                            // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                        }

                        // glDispatchCompute(groupCount, 1, 1);
                    }
                }

                Graphics::Profiler::EndAndBeginQuery("Command buffer update");

                // Prepare the command buffer
                {
                    instanceDrawCallShader.Bind();

                    int32_t drawCallCount = int32_t(indirectDrawCallBuffer.GetElementCount());

                    instanceDrawCallShader.GetUniform("drawCallCount")->SetValue(drawCallCount);

                    auto groupSize = 64;
                    auto groupCount = glm::max(drawCallCount / groupSize, 1);
                    if (drawCallCount > groupSize)
                        groupCount += groupCount % groupSize == 0 ? 0 : 1;

                    // indirectDrawCallBuffer.BindBaseAs(AE_SHADER_STORAGE_BUFFER, 7);

                    // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                    // glDispatchCompute(groupCount, 1, 1);
                }

                // Reset lod counter buffer
                if (binCounterBuffer.GetElementCount()) {
                    ResetCounterBuffer(binCounterBuffer);
                    ResetCounterBuffer(instanceCounterBuffer);
                }

                Graphics::Profiler::EndQuery();
                Graphics::Profiler::EndQuery();
                 */

            }

            Buffer::Buffer* VegetationHelper::GetCommandBuffer() {
                
                return &indirectDrawCallBuffer;

            }

            size_t VegetationHelper::GetCommandBufferOffset(Mesh::VegetationMesh& mesh,
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

            void VegetationHelper::GenerateBuffers(Scene::Vegetation& vegetation) {

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
                    meshToIdxMap[mesh] = int32_t(idx);

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
                ResetCounterBuffer(binCounterBuffer);
                
                instanceCounterBuffer.SetSize(size_t(meshCounter));
                ResetCounterBuffer(instanceCounterBuffer);

                meshInformationBuffer.SetSize(meshInformation.size(), 
                    meshInformation.data());
                meshSubdataInformationBuffer.SetSize(meshSubdataInformation.size(),
                    meshSubdataInformation.data());

                // glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            }

            void VegetationHelper::ResetCounterBuffer(Buffer::Buffer& buffer) {

                uint32_t zero = 0;
                /*
                buffer.Bind();
                buffer.InvalidateData();
                buffer.ClearData(AE_R32UI, GL_UNSIGNED_INT, &zero);
                */
            }

        }

    }

}