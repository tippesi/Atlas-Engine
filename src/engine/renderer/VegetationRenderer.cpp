#include "VegetationRenderer.h"
#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        VegetationRenderer::VegetationRenderer() {

        }

        void VegetationRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
            std::unordered_map<void*, uint16_t>& materialMap) {

            struct alignas(16) PushConstants {
                uint32_t invertUVs;
                uint32_t twoSided;
                uint32_t materialIdx;
                float normalScale;
                float displacementScale;
                float windTextureLod;
                float windBendScale;
                float windWiggleScale;
            };

            if (!scene->renderState.clutter) return;

            CPUProfiler::Scope profilerScope("Vegetation");
            GPUProfiler::Scope gpuScope("Vegetation");

            auto& vegetation = *scene->renderState.clutter;

            auto meshes = vegetation.GetMeshes();

            auto commandBuffer = helper.GetCommandBuffer();
            
            // How we order the execution of rendering commands doesn't matter here
            for (auto mesh : meshes) {
                // Clutter can share meshes with the scene, the streaming might have unloaded them
                if (!mesh->IsGeometryResident())
                    continue;

                auto buffers = vegetation.GetBuffers(mesh);

                mesh->vertexArray.Bind(commandList);

                buffers->binnedInstanceData.Bind(commandList, 3, 8);

                for (auto& subData : mesh->data.subData) {

                    // We don't do any optimizations here in terms of pipeline sorting like
                    // for opaque objects. Material variety should be smaller
                    auto pipelineConfig = GetPipelineConfigForSubData(&subData, mesh, target);
                    auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

                    commandList->BindPipeline(pipeline);

                    auto material = subData.material;

                    if (material->baseColorMap.IsLoaded())
                        material->baseColorMap->Bind(commandList, 3, 0);
                    if (material->opacityMap.IsLoaded())
                        material->opacityMap->Bind(commandList, 3, 1);
                    if (material->normalMap.IsLoaded())
                        material->normalMap->Bind(commandList, 3, 2);
                    if (material->roughnessMap.IsLoaded())
                        material->roughnessMap->Bind(commandList, 3, 3);
                    if (material->metalnessMap.IsLoaded())
                        material->metalnessMap->Bind(commandList, 3, 4);
                    if (material->aoMap.IsLoaded())
                        material->aoMap->Bind(commandList, 3, 5);
                    if (material->displacementMap.IsLoaded())
                        material->displacementMap->Bind(commandList, 3, 6);

                    scene->renderState.wind.noiseMap.Bind(commandList, 3, 7);

                    auto pushConstants = PushConstants {
                        .invertUVs = mesh->invertUVs ? 1u : 0u,
                        .twoSided = material->twoSided ? 1u : 0u,
                        .materialIdx = uint32_t(materialMap[material.Get().get()]),
                        .normalScale = material->normalScale,
                        .displacementScale = material->displacementScale,
                        .windTextureLod = mesh->windNoiseTextureLod,
                        .windBendScale = mesh->windBendScale,
                        .windWiggleScale = mesh->windWiggleScale,
                    };
                    commandList->PushConstants("constants", &pushConstants);

                    auto offset = helper.GetCommandBufferOffset(mesh, subData);
                    auto stride = sizeof(Helper::VegetationHelper::DrawElementsIndirectCommand);

                    commandList->DrawIndexedIndirect(commandBuffer->Get(), stride * offset, helper.binCount, stride);
                }
            }

        }

        PipelineConfig VegetationRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            const ResourceHandle<Mesh::Mesh> &mesh, Ref<RenderTarget> target) {

            auto shaderConfig = ShaderConfig {
                {"vegetation/vegetation.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"vegetation/vegetation.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .attachmentFormats = Graphics::AttachmentFormats(target->gBufferRendering),
                .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
            };

            auto material = subData->material;

            if (material->twoSided || !mesh->cullBackFaces) {
                pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            }

            std::vector<std::string> macros;
            if (material->baseColorMap.IsLoaded()) {
                macros.push_back("BASE_COLOR_MAP");
            }
            if (material->opacityMap.IsLoaded()) {
                macros.push_back("OPACITY_MAP");
            }
            if (material->normalMap.IsLoaded()) {
                macros.push_back("NORMAL_MAP");
            }
            if (material->roughnessMap.IsLoaded()) {
                macros.push_back("ROUGHNESS_MAP");
            }
            if (material->metalnessMap.IsLoaded()) {
                macros.push_back("METALNESS_MAP");
            }
            if (material->aoMap.IsLoaded()) {
                macros.push_back("AO_MAP");
            }
            if (material->displacementMap.IsLoaded()) {
                macros.push_back("HEIGHT_MAP");
            }
            // This is a check if we have any maps at all (no macros, no maps)
            if (macros.size()) {
                macros.push_back("TEX_COORDS");
            }
            if (glm::length(material->emissiveColor) > 0.0f) {
                macros.push_back("EMISSIVE");
            }
            if (mesh->data.colors.ContainsData()) {
                macros.push_back("VERTEX_COLORS");
            }

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}
