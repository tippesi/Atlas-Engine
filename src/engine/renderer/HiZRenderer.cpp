#include "HiZRenderer.h"

namespace Atlas {

    namespace Renderer {

        void HiZRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            pipelineConfig = PipelineConfig("gbuffer/hiz.csh");

        }

        void HiZRenderer::Generate(const RenderTargetData* data, float thickness, Graphics::CommandList* commandList) {

            auto depthTexture = data->depthTexture;
            auto resolution = ivec2(depthTexture->width, depthTexture->height);

            auto& hiZImage = data->hiZTexture->image;
            auto paddedResolution = ivec2(hiZImage->width, hiZImage->height);
            auto levelCount = std::min(data->hiZLevelCount, int32_t(hiZImage->mipLevels));

            Graphics::Profiler::BeginQuery("Hierarchical depth");

            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->ImageMemoryBarrier(hiZImage, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(depthTexture->image, depthTexture->sampler, 3, 0);

            // Each dispatch writes up to six levels from a 32x32 block of its input
            const int32_t levelsPerDispatch = 6;
            for (int32_t baseLevel = 0; baseLevel < levelCount; baseLevel += levelsPerDispatch) {
                auto fromDepth = baseLevel == 0;
                auto inputLevel = baseLevel - 1;
                auto inputSize = fromDepth ? resolution :
                    max(ivec2(hiZImage->width >> inputLevel, hiZImage->height >> inputLevel), ivec2(1));
                // The first dispatch also writes the padding
                auto blockedResolution = fromDepth ? paddedResolution : inputSize;
                ivec2 groupCount = blockedResolution / 32;
                groupCount.x += ((blockedResolution.x % 32 == 0) ? 0 : 1);
                groupCount.y += ((blockedResolution.y % 32 == 0) ? 0 : 1);

                auto dispatchLevelCount = std::min(levelsPerDispatch, levelCount - baseLevel);
                for (int32_t i = 0; i < levelsPerDispatch; i++) {
                    auto level = std::min(baseLevel + i, levelCount - 1);
                    commandList->BindImage(hiZImage, 3, 2 + i, level);
                }
                // The first dispatch reads the depth texture, the input level just needs to be valid
                commandList->BindImage(hiZImage, 3, 1, fromDepth ? levelCount - 1 : inputLevel);

                PushConstants constants = {
                    .inputSize = inputSize,
                    .fromDepth = fromDepth ? 1 : 0,
                    .levelCount = dispatchLevelCount,
                    .thickness = thickness
                };
                commandList->PushConstants("constants", &constants);

                commandList->Dispatch(groupCount.x, groupCount.y, 1);

                commandList->ImageMemoryBarrier(hiZImage, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            }

            Graphics::Profiler::EndQuery();

        }

    }

}
