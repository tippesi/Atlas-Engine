#include "OpaqueRenderer.h"

#include "../Clock.h"
#include "../materialgraph/PipelineCache.h"

#include <mutex>
#include <tuple>

namespace Atlas {

    namespace Renderer {

        void OpaqueRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

        }

        void OpaqueRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, 
            RenderList* renderList, std::unordered_map<void*, uint16_t>& materialMap, bool lateDraws) {

            CPUProfiler::Scope profilerScope("Opaque geometry");
            GPUProfiler::Scope gpuScope("Opaque geometry");

            auto mainPass = lateDraws ? renderList->GetMainPass() : renderList->PopPassFromQueue(RenderList::RenderPassType::Main);
            if (!mainPass)
                return;

            auto sceneState = &scene->renderState;

            commandList->BindBuffer(mainPass->currentMatricesBuffer, 1, 1);
            commandList->BindBuffer(mainPass->lastMatricesBuffer, 1, 2);
            commandList->BindBuffer(mainPass->currentJointMatricesBuffer, 1, 4);
            commandList->BindBuffer(mainPass->lastJointMatricesBuffer, 1, 5);
            if (mainPass->softBodyVertexOffsetsBuffer) {
                commandList->BindBuffer(renderList->currentSoftBodyVerticesBuffer, 1, 24);
                commandList->BindBuffer(renderList->lastSoftBodyVerticesBuffer, 1, 25);
                commandList->BindBuffer(mainPass->softBodyVertexOffsetsBuffer, 1, 26);
            }

            // Bind wind map
            scene->renderState.wind.noiseMap.Bind(commandList, 3, 8);

            if (!sceneState->materialGraphParameters.empty())
                sceneState->materialGraphBuffer.Bind(commandList, 3, 9);

            auto createConfig = [&](Mesh::MeshSubData* subData, Mesh::Mesh* mesh, const MaterialGraph::CompiledGraph* graph) {
                return GetPipelineConfigForSubData(subData, mesh, target, graph);
            };
            auto hasGraphPipeline = [](const MaterialGraph::CompiledGraph& graph) { return !graph.mainVertexFile.empty(); };
            drawer.Prepare(renderList, *mainPass, lateDraws ? mainPass->gpuLateDraws : mainPass->gpuDraws, !lateDraws,
                &Mesh::MeshSubData::mainPipelines, createConfig, hasGraphPipeline);

            PushConstants pushConstants;
            auto fillPushConstants = [&](const Helper::SubDataDrawer::Entry& entry, const RenderList::MeshInstances& instances,
                bool usesGraph) -> PushConstants& {
                const auto& mesh = entry.mesh;
                const auto& material = entry.subData->material;

                auto materialIt = materialMap.find(material.Get().get());
                auto materialIdx = materialIt != materialMap.end() ? uint32_t(materialIt->second) : 0u;
                auto textureIndices = materialIdx < sceneState->materialTextureIndices.size() ?
                    sceneState->materialTextureIndices[materialIdx] : Scene::MaterialTextureIndices();

                pushConstants = PushConstants {
                    .vegetation = mesh->vegetation ? 1u : 0u,
                    .invertUVs = mesh->invertUVs ? 1u : 0u,
                    .twoSided = material->twoSided ? 1u : 0u,
                    .staticMesh = mesh->mobility == Mesh::MeshMobility::Stationary ? 1u : 0u,
                    .materialIdx = materialIdx,
                    .normalScale = material->normalScale,
                    .displacementScale = material->displacementScale,
                    .windTextureLod = mesh->windNoiseTextureLod,
                    .windBendScale = mesh->windBendScale,
                    .windWiggleScale = mesh->windWiggleScale,
                    .uvAnimationX = material->uvAnimation.x,
                    .uvAnimationY = material->uvAnimation.y,
                    .uvTiling = material->tiling,
                    .baseColorTextureIdx = textureIndices.baseColor,
                    .opacityTextureIdx = textureIndices.opacity,
                    .normalTextureIdx = textureIndices.normal,
                    .roughnessTextureIdx = textureIndices.roughness,
                    .metalnessTextureIdx = textureIndices.metalness,
                    .aoTextureIdx = textureIndices.ao,
                    .heightTextureIdx = textureIndices.height,
                    .emissiveTextureIdx = textureIndices.emissive,
                    .deformOffset = uint32_t(instances.deformOffset),
                    .jointCount = uint32_t(mesh->data.skeleton.GetJointCount()),
                    .instanceOffset = uint32_t(instances.offset),
                };

                if (usesGraph) {
                    auto graphData = sceneState->materialGraphMap.find(material.Get().get());
                    if (graphData != sceneState->materialGraphMap.end()) {
                        pushConstants.materialIdx = graphData->second.materialIdx;
                        pushConstants.materialGraphOffset = graphData->second.parameterOffset;
                    }
                }
                return pushConstants;
            };

            // The level of detail is only read to color the levels
            auto setLod = [&](uint32_t lod) {
                if (pushConstants.lod == lod)
                    return false;
                pushConstants.lod = lod;
                return true;
            };

            drawer.Draw(commandList, renderList, *mainPass, &Mesh::MeshSubData::mainPipelines, fillPushConstants, setLod);

        }

        PipelineConfig OpaqueRenderer::GetPipelineConfigForSubData(Mesh::MeshSubData *subData,
            Mesh::Mesh* mesh, const Ref<RenderTarget>& target, const MaterialGraph::CompiledGraph* graph) {

            auto shaderConfig = ShaderConfig {
                {graph ? graph->mainVertexFile : "deferred/geometry.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {graph ? graph->mainFragmentFile : "deferred/geometry.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .attachmentFormats = Graphics::AttachmentFormats(target->gBufferRendering),
                .vertexInputInfo = mesh->vertexArray.GetVertexInputState(),
            };

            auto material = subData->material;

            if (material->twoSided || !mesh->cullBackFaces) {
                pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;
            }

            bool hasTangents = mesh->data.tangents.ContainsData();
            bool hasTexCoords = mesh->data.texCoords.ContainsData();

            // The surface of a graph replaces all the maps
            auto usesMaps = !graph || !graph->hasSurface;

            std::vector<std::string> macros;
            // Pipelines are rebound when the variant hash changes, which doesn't know about shaders
            // Regular materials are sorted by it, so it has to stay as it is for them
            if (graph) {
                macros.push_back("MATERIAL_GRAPH_" + std::to_string(graph->hash));
            }
            if (usesMaps && material->HasBaseColorMap() && hasTexCoords) {
                macros.push_back("BASE_COLOR_MAP");
            }
            if (usesMaps && material->HasOpacityMap() && hasTexCoords) {
                macros.push_back("OPACITY_MAP");
            }
            if (usesMaps && material->HasNormalMap() && hasTangents && hasTexCoords) {
                macros.push_back("NORMAL_MAP");
            }
            if (usesMaps && material->HasRoughnessMap() && hasTexCoords) {
                macros.push_back("ROUGHNESS_MAP");
            }
            if (usesMaps && material->HasMetalnessMap() && hasTexCoords) {
                macros.push_back("METALNESS_MAP");
            }
            if (usesMaps && material->HasAoMap() && hasTexCoords) {
                macros.push_back("AO_MAP");
            }
            if (usesMaps && material->HasDisplacementMap() && hasTangents && hasTexCoords) {
                macros.push_back("HEIGHT_MAP");
            }
            if (usesMaps && material->HasEmissiveMap() && hasTexCoords) {
                macros.push_back("EMISSIVE_MAP");
            }
            // This is a check if we have any maps at all (no macros, no maps)
            if (hasTexCoords) {
                macros.push_back("TEX_COORDS");
            }
            if (hasTangents) {
                macros.push_back("TANGENTS");
            }
            if (usesMaps && glm::length(material->emissiveColor) > 0.0f) {
                macros.push_back("EMISSIVE");
            }
            if (mesh->data.colors.ContainsData()) {
                macros.push_back("VERTEX_COLORS");
            }
            if (graph && mesh->data.texCoords1.ContainsData()) {
                macros.push_back("TEX_COORDS_1");
            }
            if (mesh->data.IsSkinned()) {
                macros.push_back("SKINNED");
            }

            if (!mesh->data.IsSkinned()) {
                macros.push_back("INSTANCE_BUFFER");
            }

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}
