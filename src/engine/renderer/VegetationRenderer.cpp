#include "VegetationRenderer.h"
#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        VegetationRenderer::VegetationRenderer() {

            /*
            depthShader.AddStage(AE_VERTEX_STAGE, "vegetation/depth.vsh");
            depthShader.AddStage(AE_FRAGMENT_STAGE, "vegetation/depth.fsh");

            depthShader.AddMacro("OPACITY_MAP");
            depthShader.Compile();
             */

        }

        void VegetationRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
            std::unordered_map<void*, uint16_t> materialMap) {

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

            if (!scene->clutter) return;

            Graphics::Profiler::BeginQuery("Vegetation");

            auto& vegetation = *scene->clutter;

            auto meshes = vegetation.GetMeshes();

            auto commandBuffer = helper.GetCommandBuffer();
            
            // DepthPrepass(vegetation, meshes, camera, time, deltaTime);

            // How we order the execution of rendering commands doesn't matter here
            for (auto mesh : meshes) {
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

                    if (material->HasBaseColorMap())
                        material->baseColorMap->Bind(commandList, 3, 0);
                    if (material->HasOpacityMap())
                        material->opacityMap->Bind(commandList, 3, 1);
                    if (material->HasNormalMap())
                        material->normalMap->Bind(commandList, 3, 2);
                    if (material->HasRoughnessMap())
                        material->roughnessMap->Bind(commandList, 3, 3);
                    if (material->HasMetalnessMap())
                        material->metalnessMap->Bind(commandList, 3, 4);
                    if (material->HasAoMap())
                        material->aoMap->Bind(commandList, 3, 5);
                    if (material->HasDisplacementMap())
                        material->displacementMap->Bind(commandList, 3, 6);

                    scene->wind.noiseMap.Bind(commandList, 3, 7);

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

            Graphics::Profiler::EndQuery();

        }

        void VegetationRenderer::DepthPrepass(Scene::Clutter& vegetation, std::vector<Mesh::Mesh*>& meshes,
            const CameraComponent& camera, float time, float deltaTime) {

            /*
            glColorMask(false, false, false, false);

            depthShader.Bind();

            depthShader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
            depthShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);

            depthShader.GetUniform("time")->SetValue(time);
            depthShader.GetUniform("deltaTime")->SetValue(deltaTime);

            glMemoryBarrier(GL_COMMAND_BARRIER_BIT);

            // How we order the execution of rendering commands doesn't matter here
            for (auto mesh : meshes) {
                mesh->Bind();
                auto buffers = vegetation.GetBuffers(mesh);

                buffers->binnedInstanceData.BindBase(5);

                depthShader.GetUniform("invertUVs")->SetValue(mesh->invertUVs);

                for (auto& subData : mesh->data.subData) {
                    auto material = subData.material;

                    if (material->HasOpacityMap())
                        material->opacityMap->Bind(1);

                    auto offset = helper.GetCommandBufferOffset(*mesh, subData);
                    glMultiDrawElementsIndirect(mesh->data.primitiveType, mesh->data.indices.GetType(),
                        (void*)(sizeof(Helper::VegetationHelper::DrawElementsIndirectCommand) * offset),
                        helper.binCount, 0);
                }
            }

            glColorMask(true, true, true, true);
             */

        }

        PipelineConfig VegetationRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            const ResourceHandle<Mesh::Mesh> &mesh, Ref<RenderTarget> target) {

            auto shaderConfig = ShaderConfig {
                {"vegetation/vegetation.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"vegetation/vegetation.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .frameBuffer = target->gBufferFrameBuffer,
                .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
            };

            auto material = subData->material;

            if (material->twoSided || !mesh->cullBackFaces) {
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
            if (mesh->data.colors.ContainsData()) {
                macros.push_back("VERTEX_COLORS");
            }

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}
