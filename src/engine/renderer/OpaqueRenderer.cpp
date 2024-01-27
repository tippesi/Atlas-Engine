#include "OpaqueRenderer.h"

#include "../Clock.h"

#include <mutex>

namespace Atlas {

    namespace Renderer {

        void OpaqueRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

        }

        void OpaqueRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, 
            RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap) {

            Graphics::Profiler::BeginQuery("Opaque geometry");

            auto mainPass = renderList->GetMainPass();

            // Retrieve all possible materials
            std::vector<std::pair<Mesh::MeshSubData*, ResourceHandle<Mesh::Mesh>>> subDatas;
            for (auto& [meshId, _] : mainPass->meshToInstancesMap) {

                auto mesh = mainPass->meshIdToMeshMap[meshId];
                for (auto& subData : mesh->data.subData) {
                    subDatas.push_back({ &subData, mesh });
                }
            }

            // Check whether materials have pipeline configs
            for (auto [subData, mesh] : subDatas) {
                auto material = subData->material;
                if (material->mainConfig.IsValid()) continue;

                material->mainConfig = GetPipelineConfigForSubData(subData, mesh, target);
            }

            // Sort materials by hash
            std::sort(subDatas.begin(), subDatas.end(),
                [](auto subData0, auto subData1) {
                return subData0.first->material->mainConfig.variantHash <
                    subData1.first->material->mainConfig.variantHash;
            });

            size_t prevHash = 0;
            Ref<Graphics::Pipeline> currentPipeline;
            ResourceHandle<Mesh::Mesh> prevMesh;
            for (auto [subData, mesh] : subDatas) {
                auto material = subData->material;
                if (material->mainConfig.variantHash != prevHash) {
                    currentPipeline = PipelineManager::GetPipeline(material->mainConfig);
                    commandList->BindPipeline(currentPipeline);
                    prevHash = material->mainConfig.variantHash;
                }

                if (mesh.GetID() != prevMesh.GetID()) {
                    mesh->vertexArray.Bind(commandList);
                    prevMesh = mesh;
                }

                auto& instance = mainPass->meshToInstancesMap[mesh.GetID()];

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
                    .vegetation = mesh->vegetation ? 1u : 0u,
                    .invertUVs = mesh->invertUVs ? 1u : 0u,
                    .twoSided = material->twoSided ? 1u : 0u,
                    .staticMesh = mesh->mobility == Mesh::MeshMobility::Stationary ? 1u : 0u,
                    .materialIdx = uint32_t(materialMap[material.get()]),
                    .normalScale = material->normalScale,
                    .displacementScale = material->displacementScale,
                    .windTextureLod = mesh->windNoiseTextureLod,
                    .windBendScale = mesh->windBendScale,
                    .windWiggleScale = mesh->windWiggleScale
                };
                commandList->PushConstants("constants", &pushConstants);

                if(!instance.count) continue;
                commandList->DrawIndexed(subData->indicesCount, instance.count, subData->indicesOffset,
                    0, instance.offset);

            }

            Graphics::Profiler::EndQuery();

        }

        PipelineConfig OpaqueRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            ResourceHandle<Mesh::Mesh>& mesh, Ref<RenderTarget> target) {

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

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}