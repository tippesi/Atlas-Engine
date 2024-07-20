#include "OpaqueRenderer.h"

#include "../Clock.h"

#include <mutex>
#include <tuple>

namespace Atlas {

    namespace Renderer {

        void OpaqueRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

        }

        void OpaqueRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, 
            RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap) {

            Graphics::Profiler::BeginQuery("Opaque geometry");

            auto mainPass = renderList->GetMainPass();
            if (!mainPass)
                return;

            commandList->BindBuffer(mainPass->currentMatricesBuffer, 1, 1);
            commandList->BindBuffer(mainPass->lastMatricesBuffer, 1, 2);
            commandList->BindBuffer(mainPass->impostorMatricesBuffer, 1, 3);

            // Bind wind map
            scene->wind.noiseMap.Bind(commandList, 3, 7);

            int32_t subDataCount = 0;
            // Retrieve all possible materials;
            for (const auto& [meshId, instances] : mainPass->meshToInstancesMap) {
                if (!instances.count) continue;

                auto& mesh = mainPass->meshIdToMeshMap[meshId];
                for (auto& subData : mesh->data.subData) {
                    if (subDataCount < subDatas.size())
                        subDatas[subDataCount] = { &subData, mesh.GetID(), mesh.Get().get() };
                    else
                        subDatas.push_back({ &subData, mesh.GetID(), mesh.Get().get() });
                    subDataCount++;
                }
            }

            // Check whether materials have pipeline configs
            for (int32_t i = 0; i < subDataCount; i++) {
                auto& [subData, _, mesh] = subDatas[i];
                const auto& material = subData->material;
                if (material->mainConfig.IsValid()) continue;

                material->mainConfig = GetPipelineConfigForSubData(subData, mesh, target);
            }

            // Sort materials by hash
            std::sort(subDatas.begin(), subDatas.begin() + size_t(subDataCount),
                [](auto& subData0, auto& subData1) {
                    return std::get<0>(subData0)->material->mainConfig.variantHash <
                        std::get<0>(subData1)->material->mainConfig.variantHash;
                });

            Hash prevMesh = 0;
            Hash prevHash = 0;
            Ref<Graphics::Pipeline> currentPipeline;
            for (int32_t i = 0; i < subDataCount; i++) {
                auto& [subData, meshID, mesh] = subDatas[i];
                const auto& instances = mainPass->meshToInstancesMap[meshID];

                const auto material = subData->material;

                // Overwrite material config frame buffer (and assume these have the same format)
                material->mainConfig.graphicsPipelineDesc.frameBuffer = target->gBufferFrameBuffer;

                if (material->mainConfig.variantHash != prevHash) {
                    currentPipeline = PipelineManager::GetPipeline(material->mainConfig);
                    commandList->BindPipeline(currentPipeline);
                    prevHash = material->mainConfig.variantHash;
                }

                if (meshID != prevMesh) {
                    mesh->vertexArray.Bind(commandList);
                    prevMesh = meshID;
                }

#if !defined(AE_BINDLESS) || defined(AE_OS_MACOS)
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
#endif

                auto pushConstants = PushConstants {
                    .vegetation = mesh->vegetation ? 1u : 0u,
                    .invertUVs = mesh->invertUVs ? 1u : 0u,
                    .twoSided = material->twoSided ? 1u : 0u,
                    .staticMesh = mesh->mobility == Mesh::MeshMobility::Stationary ? 1u : 0u,
                    .materialIdx = uint32_t(materialMap[material.get()]),
                    .normalScale = material->normalScale,
                    .displacementScale = material->displacementScale,
                    .windTextureLod = mesh->windNoiseTextureLod,
                    .windBendScale = mesh->windBendScale,
                    .windWiggleScale = mesh->windWiggleScale,
                    .baseColorTextureIdx = material->HasBaseColorMap() ? scene->textureToBindlessIdx[material->baseColorMap] : 0,
                    .opacityTextureIdx = material->HasOpacityMap() ? scene->textureToBindlessIdx[material->opacityMap] : 0,
                    .normalTextureIdx = material->HasNormalMap() ? scene->textureToBindlessIdx[material->normalMap] : 0,
                    .roughnessTextureIdx = material->HasRoughnessMap() ? scene->textureToBindlessIdx[material->roughnessMap] : 0,
                    .metalnessTextureIdx = material->HasMetalnessMap() ? scene->textureToBindlessIdx[material->metalnessMap] : 0,
                    .aoTextureIdx = material->HasAoMap() ? scene->textureToBindlessIdx[material->aoMap] : 0,
                    .heightTextureIdx = material->HasDisplacementMap() ? scene->textureToBindlessIdx[material->displacementMap] : 0,
                };
                commandList->PushConstants("constants", &pushConstants);

                commandList->DrawIndexed(subData->indicesCount, instances.count, subData->indicesOffset,
                    0, instances.offset);

                // Reset the frame buffer here to let it be able to be deleted
                material->mainConfig.graphicsPipelineDesc.frameBuffer = nullptr;

            }

            Graphics::Profiler::EndQuery();

        }

        PipelineConfig OpaqueRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            Mesh::Mesh* mesh, const Ref<RenderTarget>& target) {

            auto shaderConfig = ShaderConfig {
                {"deferred/geometry.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"deferred/geometry.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
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

#if defined(AE_BINDLESS) && !defined(AE_OS_MACOS)
            macros.push_back("BINDLESS_TEXTURES");
#endif

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}