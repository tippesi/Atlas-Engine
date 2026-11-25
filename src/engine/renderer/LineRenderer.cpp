#include "LineRenderer.h"

#include <glm/gtc/packing.hpp>

namespace Atlas {

    namespace Renderer {

        void LineRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

        }

        void LineRenderer::Render(Texture::Texture2D* texture, Texture::Texture2D* depthTexture, const CameraComponent& camera,
            const PrimitiveBatch& batch, float occludedAlpha, Graphics::CommandList* commandList) {

            auto vertices = batch.GetLineVertices();
            auto colors = batch.GetLineColors();

            // Lines are cut at the near plane, the part behind it would be mirrored through the camera
            std::vector<GPULine> gpuLines;
            gpuLines.reserve(vertices.size() / 2);
            auto nearZ = -camera.nearPlane;
            for (size_t i = 0; i + 1 < vertices.size(); i += 2) {
                auto from = vec3(camera.viewMatrix * vec4(vertices[i], 1.0f));
                auto to = vec3(camera.viewMatrix * vec4(vertices[i + 1], 1.0f));
                auto fromColor = colors[i];
                auto toColor = colors[i + 1];

                if (from.z > nearZ && to.z > nearZ)
                    continue;
                if (from.z > nearZ || to.z > nearZ) {
                    auto t = (nearZ - from.z) / (to.z - from.z);
                    auto cutPosition = glm::mix(from, to, t);
                    auto cutColor = glm::mix(fromColor, toColor, t);
                    if (from.z > nearZ) {
                        from = cutPosition;
                        fromColor = cutColor;
                    }
                    else {
                        to = cutPosition;
                        toColor = cutColor;
                    }
                }

                gpuLines.push_back(GPULine {
                    .from = vec4(from, 1.0f),
                    .to = vec4(to, 1.0f),
                    .fromColor = glm::packUnorm4x8(vec4(glm::clamp(fromColor, 0.0f, 1.0f), 1.0f)),
                    .toColor = glm::packUnorm4x8(vec4(glm::clamp(toColor, 0.0f, 1.0f), 1.0f))
                });
            }

            if (gpuLines.empty())
                return;

            bool localCommandList = !commandList;
            if (localCommandList) {
                commandList = device->GetCommandList(Graphics::GraphicsQueue);
                commandList->BeginCommands();
            }

            auto lineBuffer = uploadBufferPool.Upload(gpuLines.data(), gpuLines.size() * sizeof(GPULine));

            commandList->ImageMemoryBarrier(depthTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            auto rendering = Graphics::RenderingDesc {
                .colorAttachments = { {.image = texture->image} }
            };
            commandList->BeginRendering(rendering);

            auto pipelineConfig = GetPipelineConfig(rendering);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            const auto& projection = camera.unjitterdProjection;
            auto constants = PushConstants {
                .pMatrix = projection,
                .invResolution = 1.0f / vec2(float(texture->width), float(texture->height)),
                .depthParameters = vec2(projection[2][2], projection[3][2]),
                .width = batch.GetLineWidth(),
                .occludedAlpha = occludedAlpha,
                .testDepth = batch.testDepth ? 1u : 0u
            };
            commandList->PushConstants("constants", &constants);

            commandList->BindBuffer(lineBuffer, 3, 1);
            depthTexture->Bind(commandList, 3, 2);

            commandList->Draw(4, uint32_t(gpuLines.size()));

            commandList->EndRendering();

            if (localCommandList) {
                commandList->EndCommands();
                device->SubmitCommandList(commandList);
            }

        }

        PipelineConfig LineRenderer::GetPipelineConfig(const Graphics::RenderingDesc& rendering) {

            auto shaderConfig = ShaderConfig {
                {"line.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"line.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .attachmentFormats = Graphics::AttachmentFormats(rendering),
                .assemblyInputInfo = Graphics::Initializers::InitPipelineInputAssemblyStateCreateInfo(
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP),
                .depthStencilInputInfo = Graphics::Initializers::InitPipelineDepthStencilStateCreateInfo(
                    false, false, VK_COMPARE_OP_LESS_OR_EQUAL)
            };
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;

            // The alpha of the target stays, it isn't coverage
            auto& blend = pipelineDesc.colorBlendAttachment;
            blend.blendEnable = VK_TRUE;
            blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

            return PipelineConfig(shaderConfig, pipelineDesc);

        }

    }

}
