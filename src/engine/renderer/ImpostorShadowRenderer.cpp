#include "ImpostorShadowRenderer.h"

#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void ImpostorShadowRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

        }

        void ImpostorShadowRenderer::Render(const Ref<Graphics::FrameBuffer>& frameBuffer,
            Graphics::CommandList* commandList, RenderList* renderList, const RenderList::Pass* renderPass,
            mat4 lightViewMatrix, mat4 lightProjectionMatrix, vec3 lightLocation) {

            struct alignas(16) PushConstants {
                mat4 lightSpaceMatrix;

                vec4 lightLocation;
                vec4 lightUp;
                vec4 lightRight;
            };

            vertexArray.Bind(commandList);

            auto lightSpaceMatrix = lightProjectionMatrix * lightViewMatrix;

            // The instances are read from the instance buffer, except the ones of skinned meshes
            auto bindImpostor = [&](Mesh::Mesh* mesh, bool useInstanceBuffer) {
                auto config = GetPipelineConfig(frameBuffer, mesh->impostor->interpolation, mesh->impostor->pixelDepthOffset,
                    useInstanceBuffer);
                auto pipeline = PipelineManager::GetPipeline(config);

                commandList->BindPipeline(pipeline);

                mesh->impostor->baseColorTexture->Bind(commandList, 3, 0);
                mesh->impostor->depthTexture->Bind(commandList, 3, 1);
                mesh->impostor->viewPlaneBuffer.Bind(commandList, 3, 2);
                mesh->impostor->impostorInfoBuffer.Bind(commandList, 3, 3);

                PushConstants constants = {
                    .lightSpaceMatrix = lightSpaceMatrix,

                    .lightLocation = vec4(lightLocation, 1.0f),
                    .lightUp = glm::transpose(lightViewMatrix)[1],
                    .lightRight = glm::transpose(lightViewMatrix)[0]
                };
                commandList->PushConstants("constants", &constants);
            };

            for (const auto& item : renderPass->meshToInstancesMap) {
                auto meshId = item.first;
                auto instance = item.second;

                 // If there aren't any impostors there won't be a buffer
                if (!instance.impostorCount)
                    continue;

                const auto& mesh = renderList->meshIdToMeshMap[meshId];

                bindImpostor(mesh.Get().get(), !mesh->data.IsSkinned());
                commandList->Draw(4, instance.impostorCount, 0, instance.impostorOffset);
            }

            for (const auto& draw : renderPass->gpuDraws) {
                if (draw.type != RenderList::GPUDrawType::Impostor)
                    continue;

                bindImpostor(draw.mesh, true);
                renderList->DrawGPUCulled(commandList, draw);
            }

        }

        PipelineConfig ImpostorShadowRenderer::GetPipelineConfig(const Ref<Graphics::FrameBuffer> &frameBuffer,
            bool interpolation, bool pixelDepthOffset, bool useInstanceBuffer) {

            auto shaderConfig = ShaderConfig {
                {"impostor/impostorShadow.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"impostor/impostorShadow.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .frameBuffer = frameBuffer,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;

            std::vector<std::string> macros;
            if (interpolation) macros.push_back("INTERPOLATION");
            if (pixelDepthOffset) macros.push_back("PIXEL_DEPTH_OFFSET");
            if (useInstanceBuffer) macros.push_back("INSTANCE_BUFFER");

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}