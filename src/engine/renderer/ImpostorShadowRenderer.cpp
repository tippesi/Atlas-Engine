#include "ImpostorShadowRenderer.h"

#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void ImpostorShadowRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

        }

        void ImpostorShadowRenderer::Render(Ref<Graphics::FrameBuffer>& frameBuffer, RenderList* renderList,
            Graphics::CommandList* commandList, RenderList::Pass* renderPass,
            mat4 lightSpaceMatrix, vec3 lightLocation) {

            struct alignas(16) PushConstants {
                mat4 lightSpaceMatrix = mat4(1.0f);

                vec4 lightLocation = vec4(0.0f);
                vec4 center = vec4(0.0f);

                float radius = 1.0f;
                int32_t views = 1;
                float cutoff = 1.0f;
                float mipBias = -1.0f;
            };

            vertexArray.Bind(commandList);

            for (auto& item : renderPass->meshToInstancesMap) {
                auto meshId = item.first;
                auto instance = item.second;

                auto mesh = renderPass->meshIdToMeshMap[meshId];

                // If there aren't any impostors there won't be a buffer
                if (!instance.impostorCount)
                    continue;

                auto config = GetPipelineConfig(frameBuffer, mesh->impostor->interpolation, mesh->impostor->pixelDepthOffset);
                auto pipeline = PipelineManager::GetPipeline(config);

                commandList->BindPipeline(pipeline);

                mesh->impostor->baseColorTexture.Bind(commandList, 3, 0);
                // Base 0 is used by the materials
                mesh->impostor->viewPlaneBuffer.Bind(commandList, 3, 1);
                mesh->impostor->depthTexture.Bind(commandList, 3, 2);

                PushConstants constants = {
                    .lightSpaceMatrix = lightSpaceMatrix,

                    .lightLocation = vec4(lightLocation, 1.0f),
                    .center = vec4(mesh->impostor->center, 0.0f),

                    .radius = mesh->impostor->radius,
                    .views = mesh->impostor->views,
                    .cutoff = mesh->impostor->cutoff,

                    .mipBias = mesh->impostor->mipBias
                };
                commandList->PushConstants("constants", &constants);

                commandList->Draw(4, instance.impostorCount, 0, instance.impostorOffset);
            }

        }

        PipelineConfig ImpostorShadowRenderer::GetPipelineConfig(Ref<Graphics::FrameBuffer> &frameBuffer,
            bool interpolation, bool pixelDepthOffset) {

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

            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}