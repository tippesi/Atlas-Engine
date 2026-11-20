#include "TerrainShadowRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void TerrainShadowRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

        }

        void TerrainShadowRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            if (!scene->renderState.terrain.IsLoaded())
                return;

            CPUProfiler::Scope profilerScope("Terrain shadows");
            GPUProfiler::Scope gpuScope("Terrain shadows");

            auto terrain = scene->renderState.terrain;

            terrain->distanceVertexArray.Bind(commandList);

            for (const auto& [lightEntity, light, updateShadow] : scene->renderState.allLights) {

                if (!updateShadow || !light.shadow->allowTerrain)
                    continue;

                auto shadow = light.shadow;
                auto rendering = Graphics::RenderingDesc{
                    .depthAttachment = {.image = shadow->useCubemap ? shadow->cubemap->image : shadow->maps->image}
                };

                // We don't want to render to the long range component if it exists
                auto componentCount = shadow->viewCount;

                for (int32_t i = 0; i < componentCount; i++) {

                    auto component = &shadow->views[i];

                    rendering.depthAttachment.layer = uint32_t(i);

                    commandList->BeginRendering(rendering);

                    auto config = GeneratePipelineConfig(Graphics::AttachmentFormats(rendering), terrain.Get());
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

                        node->cell->heightField->Bind(commandList, 3, 0);

                        auto tileScale = terrain->resolution * powf(2.0f,
                            (float)(terrain->LoDCount - node->cell->LoD) - 1.0f);

                        PushConstants constants = {
                            .lightSpaceMatrix = lightSpace,
                            .translation = vec4(terrain->translation, 1.0f),

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

                    commandList->EndRendering();

                }

            }

        }

        PipelineConfig TerrainShadowRenderer::GeneratePipelineConfig(const Graphics::AttachmentFormats& attachmentFormats,
            Ref<Terrain::Terrain> &terrain) {

            const auto shaderConfig = ShaderConfig {
                {"terrain/shadowmapping.vsh", VK_SHADER_STAGE_VERTEX_BIT},
            };

            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .attachmentFormats = attachmentFormats,
                .vertexInputInfo = terrain->distanceVertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;

            return PipelineConfig(shaderConfig, pipelineDesc);

        }

    }

}