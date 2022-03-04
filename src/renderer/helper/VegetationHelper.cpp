#include "VegetationHelper.h"

namespace Atlas {

	namespace Renderer {

		namespace Helper {

			VegetationHelper::VegetationHelper() {

				instanceCullingShader.AddStage(AE_COMPUTE_STAGE, "vegetation/instanceCulling.csh");
				instanceCullingShader.Compile();

				instanceDataShader.AddStage(AE_COMPUTE_STAGE, "vegetation/instanceData.csh");
				instanceDataShader.Compile();

				instanceDrawCallShader.AddStage(AE_COMPUTE_STAGE, "vegetation/instanceDrawCall.csh");
				instanceDrawCallShader.Compile();

				indirectDrawCallBuffer = Buffer::Buffer(AE_DRAW_INDIRECT_BUFFER, 
					sizeof(DrawElementsIndirectCommand), 0);

				meshInformationBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
					sizeof(MeshInformation), 0);

				meshSubdataInformationBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
					sizeof(MeshSubdataInformation), 0);

				lodCounterBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
					sizeof(uint32_t), 0);

			}

			void VegetationHelper::PrepareInstanceBuffer(Scene::Vegetation& vegetation, Camera* camera) {

				auto meshes = vegetation.GetMeshes();

				// Check if the vegetation meshes have changed
				size_t meshFoundCount = 0;
				for (auto mesh : meshes) meshFoundCount += meshToIdxMap.find(mesh) != meshToIdxMap.end() ? 1 : 0;
				if (meshFoundCount != meshToIdxMap.size() || meshFoundCount != meshes.size()) 
					GenerateBuffers(vegetation);

				meshInformationBuffer.BindBase(0);
				meshSubdataInformationBuffer.BindBase(1);
				lodCounterBuffer.BindBase(2);

				// Cull unseen vegetation and count lods
				{
					
				}

				int32_t instanceCount = 0;

				// Move culled per instance data into own buffer
				{
					instanceDataShader.Bind();

					instanceDataShader.GetUniform("cameraLocation")->SetValue(camera->GetLocation());
					instanceDataShader.GetUniform("frustumPlanes")->SetValue(camera->frustum.GetPlanes().data(), 6);

					for (auto mesh : meshes) {
						auto buffers = vegetation.GetBuffers(mesh);

						instanceCount = int32_t(buffers->instanceData.GetElementCount());
						auto idx = meshToIdxMap[mesh];

						buffers->instanceData.BindBase(3);
						buffers->culledInstanceData.BindBase(4);
						
						instanceDataShader.GetUniform("instanceCount")->SetValue(instanceCount);
						instanceDataShader.GetUniform("meshIdx")->SetValue(idx);

						auto groupSize = 64;
						auto groupCount = instanceCount / groupSize;
						groupCount += groupCount % groupSize == 0 ? 0 : 1;

						glDispatchCompute(groupCount, 1, 1);
					}
				}

				// Prepare the command buffer
				{
					instanceDrawCallShader.Bind();

					int32_t drawCallCount = int32_t(indirectDrawCallBuffer.GetElementCount());

					instanceDrawCallShader.GetUniform("drawCallCount")->SetValue(drawCallCount);

					auto groupSize = 64;
					auto groupCount = glm::max(drawCallCount / groupSize, 1);
					if (drawCallCount > groupSize)
						groupCount += groupCount % groupSize == 0 ? 0 : 1;

					indirectDrawCallBuffer.BindBaseAs(AE_SHADER_STORAGE_BUFFER, 5);

					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
					glDispatchCompute(groupCount, 1, 1);
				}

				// Reset lod counter buffer
				if (lodCounterBuffer.GetElementCount()) ResetCounterBuffer();

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
						return i;
					}
				}

				return 0;

			}

			void VegetationHelper::GenerateBuffers(Scene::Vegetation& vegetation) {

				auto meshes = vegetation.GetMeshes();

				// Delete old information
				meshSubdataInformation.clear();

				uint32_t idx = 0;
				uint32_t lodCounter = 0;
				std::vector<MeshInformation> meshInformation;
				for (auto mesh : meshes) {
					meshInformation.push_back({ vec4(mesh->data.aabb.min, reinterpret_cast<float&>(lodCounter)), vec4(mesh->data.aabb.max, 0.0)});
					meshToIdxMap[mesh] = int32_t(idx);

					for (auto& subdata : mesh->data.subData) {
						meshSubdataInformation.push_back({ idx,
							subdata.indicesOffset, subdata.indicesCount });
					}

					idx++;
					lodCounter++; // Needs to be removed in the future when there are lods
				}

				indirectDrawCallBuffer.SetSize(meshSubdataInformation.size());

				lodCounterBuffer.SetSize(size_t(lodCounter));
				ResetCounterBuffer();

				meshInformationBuffer.SetSize(meshInformation.size(), 
					meshInformation.data());
				meshSubdataInformationBuffer.SetSize(meshSubdataInformation.size(),
					meshSubdataInformation.data());

				glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			}

			void VegetationHelper::ResetCounterBuffer() {

				uint32_t zero = 0;
				lodCounterBuffer.Bind();
				lodCounterBuffer.InvalidateData();
				lodCounterBuffer.ClearData(AE_R32UI, GL_UNSIGNED_INT, &zero);

			}

		}

	}

}