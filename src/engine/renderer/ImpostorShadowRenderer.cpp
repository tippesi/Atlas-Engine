#include "ImpostorShadowRenderer.h"

#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void ImpostorShadowRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

        }

        void ImpostorShadowRenderer::Render(const Graphics::AttachmentFormats& attachmentFormats,
            Graphics::CommandList* commandList, RenderList* renderList, const RenderList::Pass* renderPass,
            mat4 lightViewMatrix, mat4 lightProjectionMatrix, vec3 lightLocation) {

            struct alignas(16) PushConstants {
                mat4 lightSpaceMatrix;

                vec4 lightLocation;
            };

            vertexArray.Bind(commandList);

            auto lightSpaceMatrix = lightProjectionMatrix * lightViewMatrix;

            PushConstants constants = {
                .lightSpaceMatrix = lightSpaceMatrix,

                .lightLocation = vec4(lightLocation, 1.0f)
            };

            // All impostors share a few pipeline variants, a pipeline is only looked up and bound when the variant changes
            int32_t boundVariant = -1;
            auto bindImpostor = [&](Mesh::Mesh* mesh) {
                auto& impostor = mesh->impostor;
                auto variant = int32_t(impostor->interpolation) | int32_t(impostor->pixelDepthOffset) << 1;
                if (variant != boundVariant) {
                    boundVariant = variant;
                    auto config = GetPipelineConfig(attachmentFormats, impostor->interpolation, impostor->pixelDepthOffset);
                    auto pipeline = PipelineManager::GetPipeline(config);
                    commandList->BindPipeline(pipeline);
                    commandList->PushConstants(pipeline->shader->GetPushConstantRange("constants"), &constants);
                }

                impostor->baseColorTexture->Bind(commandList, 3, 0);
                impostor->depthTexture->Bind(commandList, 3, 1);
                impostor->viewPlaneBuffer.Bind(commandList, 3, 2);
                impostor->impostorInfoBuffer.Bind(commandList, 3, 3);
            };

            for (const auto& item : renderPass->meshToInstancesMap) {
                auto meshId = item.first;
                auto instance = item.second;

                 // If there aren't any impostors there won't be a buffer
                if (!instance.impostorCount)
                    continue;

                const auto& mesh = renderList->meshIdToMeshMap[meshId];

                bindImpostor(mesh.Get().get());
                commandList->Draw(4, instance.impostorCount, 0, instance.impostorOffset);
            }

            for (const auto& draw : renderPass->gpu.draws) {
                if (draw.type != RenderList::GPUDrawType::Impostor)
                    continue;

                bindImpostor(draw.mesh);
                renderList->DrawGPUCulled(commandList, draw);
            }

        }

        PipelineConfig ImpostorShadowRenderer::GetPipelineConfig(const Graphics::AttachmentFormats& attachmentFormats,
            bool interpolation, bool pixelDepthOffset) {

            auto shaderConfig = ShaderConfig {
                {"impostor/impostorShadow.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"impostor/impostorShadow.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc{
                .attachmentFormats = attachmentFormats,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
            };

            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;

            std::vector<std::string> macros;
            if (interpolation) macros.push_back("INTERPOLATION");
            if (pixelDepthOffset) macros.push_back("PIXEL_DEPTH_OFFSET");

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}