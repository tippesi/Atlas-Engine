#include "TerrainShadowRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void TerrainShadowRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

        }

        void TerrainShadowRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            if (!scene->terrain)
                return;

            Graphics::Profiler::BeginQuery("Terrain shadows");

            auto terrain = scene->terrain;

            terrain->distanceVertexArray.Bind(commandList);

            auto lightSubset = scene->GetSubset<LightComponent>();

            LightMap usedLightMap;
            for (auto& lightEntity : lightSubset) {

                auto& light = lightEntity.GetComponent<LightComponent>();
                if (!light.shadow || !light.shadow->update || !light.shadow->allowTerrain)
                    continue;

                auto shadow = light.shadow;
                auto frameBuffer = GetOrCreateFrameBuffer(lightEntity);
                usedLightMap[lightEntity] = frameBuffer;

                if (frameBuffer->depthAttachment.layer != 0) {
                    frameBuffer->depthAttachment.layer = 0;
                    frameBuffer->Refresh();
                }

                // We don't want to render to the long range component if it exists
                auto componentCount = shadow->viewCount;

                for (int32_t i = 0; i < componentCount; i++) {

                    auto component = &shadow->views[i];

                    if (frameBuffer->depthAttachment.layer != i) {
                        frameBuffer->depthAttachment.layer = i;
                        frameBuffer->Refresh();
                    }

                    commandList->BeginRenderPass(frameBuffer->renderPass, frameBuffer);

                    auto config = GeneratePipelineConfig(frameBuffer, terrain);
                    auto pipeline = PipelineManager::GetPipeline(config);

                    commandList->BindPipeline(pipeline);

                    auto frustum = Volume::Frustum(component->terrainFrustumMatrix);

                    mat4 lightSpace = component->projectionMatrix * component->viewMatrix;
                    
                    // Use middle of near plane as camera origin
                    auto corners = frustum.GetCorners();
                    auto center = corners[4] + 0.5f * (corners[5] - corners[4])
                        + 0.5f * (corners[6] - corners[4]);

                    terrain->UpdateRenderlist(frustum, center);

                    for (auto node : terrain->renderList) {

                        node->cell->heightField.Bind(commandList, 3, 0);

                        auto tileScale = terrain->resolution * powf(2.0f,
                            (float)(terrain->LoDCount - node->cell->LoD) - 1.0f);

                        PushConstants constants = {
                            .lightSpaceMatrix = lightSpace,

                            .nodeSideLength = node->sideLength,
                            .tileScale = tileScale,
                            .patchSize = float(terrain->patchSizeFactor),
                            .heightScale = terrain->heightScale,

                            .leftLoD = node->leftLoDStitch,
                            .topLoD = node->topLoDStitch,
                            .rightLoD = node->rightLoDStitch,
                            .bottomLoD = node->bottomLoDStitch,

                            .nodeLocation = node->location
                        };

                        commandList->PushConstants("constants", &constants);

                        commandList->DrawIndexed(terrain->distanceVertexArray.GetIndexComponent().elementCount);

                    }

                    commandList->EndRenderPass();

                }

            }

            lightMap = usedLightMap;

            Graphics::Profiler::EndQuery();

        }

        PipelineConfig TerrainShadowRenderer::GeneratePipelineConfig(Ref<Graphics::FrameBuffer>& framebuffer,
            Ref<Terrain::Terrain> &terrain) {

            const auto shaderConfig = ShaderConfig {
                {"terrain/shadowmapping.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"terrain/shadowmapping.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };

            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = framebuffer,
                .vertexInputInfo = terrain->distanceVertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;

            return PipelineConfig(shaderConfig, pipelineDesc);

        }

        Ref<Graphics::FrameBuffer> TerrainShadowRenderer::GetOrCreateFrameBuffer(Scene::Entity entity) {

            auto& light = entity.GetComponent<LightComponent>();

            auto shadow = light.shadow;
            if (lightMap.contains(entity)) {
                return lightMap[entity];
            }
            else {
                Graphics::RenderPassDepthAttachment attachment = {
                    .imageFormat = shadow->useCubemap ? shadow->cubemap.format :
                                   shadow->maps.format,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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