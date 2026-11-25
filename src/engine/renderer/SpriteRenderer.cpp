#include "SpriteRenderer.h"

#include <glm/gtc/packing.hpp>

namespace Atlas {

    namespace Renderer {

        void SpriteRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

        }

        void SpriteRenderer::Render(Texture::Texture2D* texture, Texture::Texture2D* depthTexture, const CameraComponent& camera,
            std::span<const Sprite> sprites, float occludedAlpha, Graphics::CommandList* commandList) {

            // Sprites behind the near plane would be mirrored through the camera
            std::vector<GPUSprite> gpuSprites;
            std::vector<Texture::Texture2D*> textures;
            gpuSprites.reserve(sprites.size());
            textures.reserve(sprites.size());
            for (const auto& sprite : sprites) {
                auto viewPosition = vec3(camera.viewMatrix * vec4(sprite.position, 1.0f));
                if (viewPosition.z > -camera.nearPlane)
                    continue;

                gpuSprites.push_back(GPUSprite {
                    .viewPosition = vec4(viewPosition, sprite.screenSpaceSize ? 1.0f : 0.0f),
                    .uvMinMax = vec4(sprite.uvMin, sprite.uvMax),
                    .size = sprite.size,
                    .color = glm::packUnorm4x8(sprite.color),
                    .outlineColor = glm::packUnorm4x8(sprite.outlineColor),
                    .outlineWidth = sprite.outlineWidth
                });
                textures.push_back(sprite.texture);
            }

            if (gpuSprites.empty())
                return;

            bool localCommandList = !commandList;
            if (localCommandList) {
                commandList = device->GetCommandList(Graphics::GraphicsQueue);
                commandList->BeginCommands();
            }

            auto spriteBuffer = uploadBufferPool.Upload(gpuSprites.data(), gpuSprites.size() * sizeof(GPUSprite));

            commandList->ImageMemoryBarrier(depthTexture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            auto rendering = Graphics::RenderingDesc {
                .colorAttachments = { {.image = texture->image} }
            };
            commandList->BeginRendering(rendering);

            auto pipelineConfig = GetPipelineConfig(rendering);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            auto constants = PushConstants {
                .pMatrix = camera.unjitterdProjection,
                .invResolution = 1.0f / vec2(float(texture->width), float(texture->height)),
                .occludedAlpha = occludedAlpha
            };
            commandList->PushConstants("constants", &constants);

            commandList->BindBuffer(spriteBuffer, 3, 1);
            depthTexture->Bind(commandList, 3, 2);

            for (size_t first = 0; first < textures.size();) {
                auto last = first + 1;
                while (last < textures.size() && textures[last] == textures[first])
                    last++;

                textures[first]->Bind(commandList, 3, 0);
                commandList->Draw(4, uint32_t(last - first), 0, uint32_t(first));

                first = last;
            }

            commandList->EndRendering();

            if (localCommandList) {
                commandList->EndCommands();
                device->SubmitCommandList(commandList);
            }

        }

        PipelineConfig SpriteRenderer::GetPipelineConfig(const Graphics::RenderingDesc& rendering) {

            auto shaderConfig = ShaderConfig {
                {"sprite.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"sprite.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
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
