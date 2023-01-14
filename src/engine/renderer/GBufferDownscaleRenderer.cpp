#include "GBufferDownscaleRenderer.h"

namespace Atlas {

    namespace Renderer {

        void GBufferDownscaleRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            downscalePipelineConfig = PipelineConfig("downsampleGBuffer2x.csh");
            downscaleDepthOnlyPipelineConfig = PipelineConfig("downsampleGBuffer2x.csh", {"DEPTH_ONLY"});

        }

        void GBufferDownscaleRenderer::Render(Viewport *viewport, RenderTarget *target, 
            Camera *camera, Scene::Scene *scene) {


            
        }

        void GBufferDownscaleRenderer::Downscale(RenderTarget* target, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Downsample GBuffer");

            auto pipeline = PipelineManager::GetPipeline(downscalePipelineConfig);
            commandList->BindPipeline(pipeline);

            auto rt = target->GetData(RenderResolution::FULL_RES);
            auto downsampledRt = target->GetData(RenderResolution::HALF_RES);

            Downscale(rt, downsampledRt, commandList);

            Graphics::Profiler::EndQuery();

        }

        void GBufferDownscaleRenderer::DownscaleDepthOnly(RenderTarget* target, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Downsample GBuffer depth only");

            auto pipeline = PipelineManager::GetPipeline(downscaleDepthOnlyPipelineConfig);
            commandList->BindPipeline(pipeline);

            auto rt = target->GetData(RenderResolution::FULL_RES);
            auto downsampledRt = target->GetData(RenderResolution::HALF_RES);

            Downscale(rt, downsampledRt, commandList);

            Graphics::Profiler::EndQuery();

        }

        void GBufferDownscaleRenderer::Downscale(RenderTargetData* rt,
            RenderTargetData* downsampledRt, Graphics::CommandList* commandList) {

            auto depthIn = rt->depthTexture;
            auto normalIn = rt->normalTexture;
            auto geometryNormalIn = rt->geometryNormalTexture;
            auto roughnessMetallicAoIn = rt->roughnessMetallicAoTexture;
            auto velocityIn = rt->velocityTexture;
            auto materialIdxIn = rt->materialIdxTexture;

            auto depthOut = downsampledRt->depthTexture;
            auto normalOut = downsampledRt->normalTexture;
            auto geometryNormalOut = downsampledRt->geometryNormalTexture;
            auto roughnessMetallicAoOut = downsampledRt->roughnessMetallicAoTexture;
            auto velocityOut = downsampledRt->velocityTexture;
            auto materialIdxOut = downsampledRt->materialIdxTexture;
            auto offsetOut = downsampledRt->offsetTexture;

            ivec2 res = ivec2(depthOut->width, depthOut->height);

            ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
            groupCount.x += ((res.x % 8 == 0) ? 0 : 1);
            groupCount.y += ((res.y % 8 == 0) ? 0 : 1);

            // These are framebuffer attachments and should be in the right layout
            commandList->BindImage(depthIn->image, depthIn->sampler, 3 , 0);
            commandList->BindImage(normalIn->image, normalIn->sampler, 3 , 1);
            commandList->BindImage(geometryNormalIn->image, geometryNormalIn->sampler, 3 , 2);
            commandList->BindImage(roughnessMetallicAoIn->image, roughnessMetallicAoIn->sampler, 3 , 3);
            commandList->BindImage(velocityIn->image, velocityIn->sampler, 3 , 4);
            commandList->BindImage(materialIdxIn->image, materialIdxIn->sampler, 3 , 5);

            std::vector<Graphics::ImageBarrier> imageBarriers;
            std::vector<Graphics::BufferBarrier> bufferBarriers;
            imageBarriers.push_back({depthOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT});
            imageBarriers.push_back({normalOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT});
            imageBarriers.push_back({geometryNormalOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT});
            imageBarriers.push_back({roughnessMetallicAoOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT});
            imageBarriers.push_back({velocityOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT});
            imageBarriers.push_back({materialIdxOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT});
            imageBarriers.push_back({offsetOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT});

            commandList->PipelineBarrier(imageBarriers, bufferBarriers);

            commandList->BindImage(depthOut->image, 3 , 6);
            commandList->BindImage(normalOut->image, 3 , 7);
            commandList->BindImage(geometryNormalOut->image, 3 , 8);
            commandList->BindImage(roughnessMetallicAoOut->image, 3 , 9);
            commandList->BindImage(velocityOut->image, 3 , 10);
            commandList->BindImage(materialIdxOut->image, 3 , 11);
            commandList->BindImage(offsetOut->image, 3 , 12);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            imageBarriers.clear();
            imageBarriers.push_back({depthOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
            imageBarriers.push_back({normalOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
            imageBarriers.push_back({geometryNormalOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
            imageBarriers.push_back({roughnessMetallicAoOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
            imageBarriers.push_back({velocityOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
            imageBarriers.push_back({materialIdxOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
            imageBarriers.push_back({offsetOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});

            commandList->PipelineBarrier(imageBarriers, bufferBarriers);

        }

    }

}