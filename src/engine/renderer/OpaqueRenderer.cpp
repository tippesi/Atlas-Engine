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
                for (auto& subData : mesh->data->subData) {
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
                commandList->PushConstants("constants", &pushConstants);

                if(!instance.count) continue;
                commandList->DrawIndexed(subData->indicesCount, instance.count, subData->indicesOffset,
                    0, instance.offset);

            }

            Graphics::Profiler::EndQuery();

        }

        PipelineConfig OpaqueRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            Mesh::Mesh *mesh, RenderTarget *target) {

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

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}