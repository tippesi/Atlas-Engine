#include "ShadowRenderer.h"

#include "../Clock.h"

#include "../lighting/DirectionalLight.h"
#include "../lighting/PointLight.h"

namespace Atlas {

	namespace Renderer {

        void ShadowRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

        }

		void ShadowRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList, RenderList* renderList) {

            Graphics::Profiler::BeginQuery("Shadows");

            auto lights = scene->GetLights();
            if (scene->sky.sun) {
                lights.push_back(scene->sky.sun.get());
            }

            LightMap usedLightMap;

            for (auto& light : lights) {

                if (!light->GetShadow()) {
                    continue;
                }

                if (!light->GetShadow()->update) {
                    continue;
                }

                auto shadow = light->GetShadow();
                auto frameBuffer = GetOrCreateFrameBuffer(light);
                usedLightMap[light] = frameBuffer;

                if (frameBuffer->depthAttachment.layer != 0) {
                    frameBuffer->depthAttachment.layer = 0;
                    frameBuffer->Refresh();
                }

                // We don't want to render to the long range component if it exists
                auto componentCount = light->GetShadow()->longRange ?
                    light->GetShadow()->componentCount - 1 :
                    light->GetShadow()->componentCount;

                bool isDirectionalLight = false;
                vec3 lightLocation;

                if (light->type == AE_DIRECTIONAL_LIGHT) {
                    auto directionLight = static_cast<Lighting::DirectionalLight*>(light);
                    lightLocation = 1000000.0f * -normalize(directionLight->direction);
                    isDirectionalLight = true;
                }
                else if (light->type == AE_POINT_LIGHT) {
                    auto pointLight = static_cast<Lighting::PointLight*>(light);
                    lightLocation = pointLight->location;
                }

                for (uint32_t i = 0; i < uint32_t(componentCount); i++) {

                    auto component = &shadow->components[i];

                    if (frameBuffer->depthAttachment.layer != i) {
                        frameBuffer->depthAttachment.layer = i;
                        frameBuffer->Refresh();
                    }

                    auto shadowPass = renderList->GetShadowPass(light, i);

                    commandList->BeginRenderPass(frameBuffer->renderPass, frameBuffer, true);

                    // Retrieve all possible materials
                    std::vector<std::pair<Mesh::MeshSubData*, Mesh::Mesh*>> subDatas;
                    for (auto& [mesh, _] : shadowPass->meshToInstancesMap) {
                        for (auto& subData : mesh->data.subData) {
                            subDatas.push_back({ &subData, mesh });
                        }
                    }

                    // Check whether materials have pipeline configs
                    for (auto [subData, mesh] : subDatas) {
                        auto material = subData->material;
                        if (material->shadowConfig.IsValid()) continue;

                        auto shaderConfig = ShaderConfig {
                            {"shadowMapping.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                            {"shadowMapping.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
                        };
                        auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                            .frameBuffer = frameBuffer,
                            .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
                        };

                        if (!mesh->cullBackFaces) {
                            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
                        }

                        std::vector<std::string> macros;
                        if (material->HasOpacityMap()) {
                            macros.push_back("OPACITY_MAP");
                        }

                        material->shadowConfig = PipelineConfig(shaderConfig, pipelineDesc, macros);
                    }

                    // Sort materials by hash
                    std::sort(subDatas.begin(), subDatas.end(),
                        [](auto subData0, auto subData1) {
                            return subData0.first->material->shadowConfig.variantHash <
                                   subData1.first->material->shadowConfig.variantHash;
                        });

                    size_t prevHash = 0;
                    Ref<Graphics::Pipeline> currentPipeline = nullptr;
                    Mesh::Mesh* prevMesh = nullptr;
                    for (auto [subData, mesh] : subDatas) {
                        auto material = subData->material;
                        if (material->shadowConfig.variantHash != prevHash) {
                            currentPipeline = PipelineManager::GetPipeline(material->shadowConfig);
                            commandList->BindPipeline(currentPipeline);
                            prevHash = material->shadowConfig.variantHash;
                        }

                        if (mesh != prevMesh) {
                            mesh->vertexArray.Bind(commandList);
                            prevMesh = mesh;
                        }

                        auto& instance = shadowPass->meshToInstancesMap[mesh];

                        if (material->HasOpacityMap())
                            commandList->BindImage(material->opacityMap->image, material->opacityMap->sampler, 3, 0);

                        auto pushConstants = PushConstants {
                            .lightSpaceMatrix = component->projectionMatrix * component->viewMatrix,
                            .vegetation = mesh->vegetation ? 1u : 0u,
                            .invertUVs = mesh->invertUVs ? 1u : 0u
                        };

                        auto constantRange = currentPipeline->shader->GetPushConstantRange("constants");
                        commandList->PushConstants(constantRange, &pushConstants);

                        commandList->DrawIndexed(subData->indicesCount, instance.count, subData->indicesOffset,
                            0, instance.offset);

                    }

                    commandList->EndRenderPass();

                }

            }

            lightMap = usedLightMap;

            Graphics::Profiler::EndQuery();

		}

        Ref<Graphics::FrameBuffer> ShadowRenderer::GetOrCreateFrameBuffer(Lighting::Light* light) {

            auto shadow = light->GetShadow();
            if (lightMap.contains(light)) {
                return lightMap[light];
            }
            else {
                Graphics::RenderPassAttachment attachment = {
                    .imageFormat = shadow->useCubemap ? shadow->cubemap.format :
                                   shadow->maps.format,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
                Graphics::RenderPassDesc renderPassDesc = {
                    .depthAttachment = { attachment }
                };
                auto renderPass = device->CreateRenderPass(renderPassDesc);

                Graphics::FrameBufferDesc frameBufferDesc = {
                    .renderPass = renderPass,
                    .depthAttachment = { shadow->useCubemap ? shadow->cubemap.image : shadow->maps.image, 0, true},
                    .extent = { uint32_t(shadow->resolution), uint32_t(shadow->resolution) }
                };
                return device->CreateFrameBuffer(frameBufferDesc);
            }

        }

	}

}