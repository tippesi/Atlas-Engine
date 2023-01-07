#include "OpaqueRenderer.h"

#include "../Clock.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

        void OpaqueRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

		}

		void OpaqueRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
			Scene::Scene* scene, Graphics::CommandList* commandList, RenderList* renderList,
            std::unordered_map<void*, uint16_t> materialMap) {

            Graphics::Profiler::BeginQuery("Opaque geometry");

            auto mainPass = renderList->GetMainPass();

            // Retrieve all possible materials
            std::vector<std::pair<Mesh::MeshSubData*, Mesh::Mesh*>> subDatas;
            for (auto& [mesh, _] : mainPass->meshToInstancesMap) {
                for (auto& subData : mesh->data.subData) {
                    subDatas.push_back({ &subData, mesh });
                }
            }

            // Check whether materials have pipeline configs
            for (auto [subData, mesh] : subDatas) {
                auto material = subData->material;
                if (material->mainConfig.IsValid()) continue;

                auto shaderConfig = ShaderConfig {
                    {"deferred/geometry.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                    {"deferred/geometry.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
                };
                auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                    .frameBuffer = target->gBufferFrameBuffer,
                    .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
                };

                if (!mesh->cullBackFaces) {
                    pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
                }

                std::vector<std::string> macros;
                if (material->HasBaseColorMap()) {
                    macros.push_back("BASE_COLOR_MAP");
                }
                if (material->HasOpacityMap()) {
                    macros.push_back("OPACITY_MAP");
                }
                if (material->HasNormalMap()) {
                    macros.push_back("NORMAL_MAP");
                }
                if (material->HasRoughnessMap()) {
                    macros.push_back("ROUGHNESS_MAP");
                }
                if (material->HasMetalnessMap()) {
                    macros.push_back("METALNESS_MAP");
                }
                if (material->HasAoMap()) {
                    macros.push_back("AO_MAP");
                }
                if (material->HasDisplacementMap()) {
                    macros.push_back("HEIGHT_MAP");
                }
                // This is a check if we have any maps at all (no macros, no maps)
                if (macros.size()) {
                    macros.push_back("TEX_COORDS");
                }
                if (glm::length(material->emissiveColor) > 0.0f) {
                    macros.push_back("EMISSIVE");
                }

                material->mainConfig = PipelineConfig(shaderConfig, pipelineDesc, macros);
            }

            // Sort materials by hash
            std::sort(subDatas.begin(), subDatas.end(),
                [](auto subData0, auto subData1) {
                return subData0.first->material->mainConfig.variantHash <
                    subData1.first->material->mainConfig.variantHash;
            });

            size_t prevHash = 0;
            Ref<Graphics::Pipeline> currentPipeline = nullptr;
            Mesh::Mesh* prevMesh = nullptr;
            for (auto [subData, mesh] : subDatas) {
                auto material = subData->material;
                if (material->mainConfig.variantHash != prevHash) {
                    currentPipeline = PipelineManager::GetPipeline(material->mainConfig);
                    commandList->BindPipeline(currentPipeline);
                    prevHash = material->mainConfig.variantHash;
                }

                if (mesh != prevMesh) {
                    mesh->vertexArray.Bind(commandList);
                    prevMesh = mesh;
                }

                auto& instance = mainPass->meshToInstancesMap[mesh];

                if (material->HasBaseColorMap())
                    commandList->BindImage(material->baseColorMap->image, material->baseColorMap->sampler, 3, 0);
                if (material->HasOpacityMap())
                    commandList->BindImage(material->opacityMap->image, material->opacityMap->sampler, 3, 1);
                if (material->HasNormalMap())
                    commandList->BindImage(material->normalMap->image, material->normalMap->sampler, 3, 2);
                if (material->HasRoughnessMap())
                    commandList->BindImage(material->roughnessMap->image, material->roughnessMap->sampler, 3, 3);
                if (material->HasMetalnessMap())
                    commandList->BindImage(material->metalnessMap->image, material->metalnessMap->sampler, 3, 4);
                if (material->HasAoMap())
                    commandList->BindImage(material->aoMap->image, material->aoMap->sampler, 3, 5);
                if (material->HasDisplacementMap())
                    commandList->BindImage(material->displacementMap->image, material->displacementMap->sampler, 3, 6);

                auto pushConstants = PushConstants {
                    .vegetation = mesh->vegetation ? 1u : 0u,
                    .invertUVs = mesh->invertUVs ? 1u : 0u,
                    .twoSided = material->twoSided ? 1u : 0u,
                    .staticMesh = mesh->mobility == Mesh::MeshMobility::Stationary ? 1u : 0u,
                    .materialIdx = uint32_t(materialMap[material]),
                    .normalScale = material->normalScale,
                    .displacementScale = material->displacementScale
                };

                auto constantRange = currentPipeline->shader->GetPushConstantRange("constants");
                commandList->PushConstants(constantRange, &pushConstants);

                commandList->DrawIndexed(subData->indicesCount, instance.count, subData->indicesOffset,
                    0, instance.offset);

            }

            Graphics::Profiler::EndQuery();

		}

		void OpaqueRenderer::RenderImpostor(Viewport* viewport, std::vector<mat4> viewMatrices,
			mat4 projectionMatrix, Mesh::Mesh* mesh, Mesh::Impostor* impostor) {

            /*
			if (!viewMatrices.size())
				return;

			std::lock_guard<std::mutex> guard(shaderBatchMutex);

			Actor::MovableMeshActor actor(mesh);

			framebuffer->Bind(true);

			glDisable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			Camera camera;

			camera.viewMatrix = viewMatrices[0];
			camera.projectionMatrix = projectionMatrix;

			renderList.Add(&actor);
			renderList.UpdateBuffers(&camera);

			// Iterate through shaders and add macro
			for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {
				auto shaderID = renderListBatchesKey.first;
				auto renderListBatches = renderListBatchesKey.second;

				auto shader = shaderBatch.GetShader(shaderID);
				// We want normals in world space
				// And a specular map no matter what
				shader->AddMacro("GENERATE_IMPOSTOR");
				shader->Compile();
			}

			for (size_t i = 0; i < viewMatrices.size(); i++) {

				glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
				glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);		

				for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {

					auto shaderID = renderListBatchesKey.first;
					auto renderListBatches = renderListBatchesKey.second;

					auto shader = shaderBatch.GetShader(shaderID);

					shaderBatch.Bind(shaderID);

					projectionMatrixUniform->SetValue(projectionMatrix);
					viewMatrixUniform->SetValue(viewMatrices[i]);

					for (auto renderListBatch : renderListBatches) {

						auto actorBatch = renderListBatch.actorBatch;

						// If there is no actor of that mesh visible we discard it.
						if (!actorBatch->GetSize()) {
							continue;
						}

						auto mesh = actorBatch->GetObject();
						auto key = renderList.actorBatchBuffers.find(mesh);

						if (key == renderList.actorBatchBuffers.end())
							continue;

						auto buffers = key->second;

						mesh->Bind();
						buffers.currentMatrices->BindBase(2);
						if (mesh->mobility != AE_STATIONARY_MESH) buffers.lastMatrices->BindBase(3);

						invertUVsUniform->SetValue(mesh->invertUVs);
						// We *always* want to render impostors two sided
						twoSidedUniform->SetValue(true);

						// Prepare uniform buffer here
						// Generate all drawing commands
						// We could also batch several materials together because they share the same shader

						// Render the sub data of the mesh that use this specific shader
						for (auto& subData : renderListBatch.subData) {

							auto material = subData->material;

							if (material->HasBaseColorMap())
								material->baseColorMap->Bind(0);
							if (material->HasOpacityMap())
								material->opacityMap->Bind(1);
							if (material->HasNormalMap())
								material->normalMap->Bind(2);
							if (material->HasRoughnessMap())
								material->roughnessMap->Bind(3);
							if (material->HasMetalnessMap())
								material->metalnessMap->Bind(4);
							if (material->HasAoMap())
								material->aoMap->Bind(5);
							if (material->HasDisplacementMap())
								material->displacementMap->Bind(6);

							baseColorUniform->SetValue(material->baseColor);
							roughnessUniform->SetValue(material->roughness);
							metalnessUniform->SetValue(material->metalness);
							aoUniform->SetValue(material->ao);
							normalScaleUniform->SetValue(material->normalScale);
							displacementScaleUniform->SetValue(material->displacementScale);

							glDrawElementsInstanced(mesh->data.primitiveType, subData->indicesCount, mesh->data.indices.GetType(),
								(void*)((uint64_t)(subData->indicesOffset * mesh->data.indices.GetElementSize())), actorBatch->GetSize());

						}

					}

				}

				impostor->baseColorTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT0),
					0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);
				// Just use the geometry normals for now. We might add support for normal maps later.
				// For now we just assume that the geometry itself has enough detail when viewed from distance
				impostor->normalTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT2),
					0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);
				impostor->roughnessMetalnessAoTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT3),
					0, 0, 0, 0, 0, (int32_t)i, impostor->resolution, impostor->resolution, 1);

				glFlush();

			}

			// Iterate through shaders and remove macro
			for (auto& renderListBatchesKey : renderList.orderedRenderBatches) {
				auto shaderID = renderListBatchesKey.first;
				auto renderListBatches = renderListBatchesKey.second;

				auto shader = shaderBatch.GetShader(shaderID);
				shader->RemoveMacro("GENERATE_IMPOSTOR");
				shader->Compile();
			}

			renderList.Clear();
			framebuffer->Unbind();

			glEnable(GL_CULL_FACE);
             */

		}

	}

}