#include "GBufferRenderer.h"

namespace Atlas {

    namespace Renderer {

        void GBufferRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            downscalePipelineConfig = PipelineConfig("gbuffer/downsampleGBuffer2x.csh");
            downscaleDepthOnlyPipelineConfig = PipelineConfig("gbuffer/downsampleGBuffer2x.csh", {"DEPTH_ONLY"});

            patchNormalPipelineConfig = PipelineConfig("gbuffer/patchGBufferNormals.csh");
            generateReactiveMaskPipelineConfig = PipelineConfig("gbuffer/generateReactiveMask.csh");

        }

        void GBufferRenderer::Downscale(const Ref<RenderTarget>& target, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Downsample GBuffer");

            auto pipeline = PipelineManager::GetPipeline(downscalePipelineConfig);
            commandList->BindPipeline(pipeline);

            auto rt = target->GetData(RenderResolution::FULL_RES);
            auto downsampledRt = target->GetData(RenderResolution::HALF_RES);

            Downscale(rt, downsampledRt, commandList);

            Graphics::Profiler::EndQuery();

        }

        void GBufferRenderer::DownscaleDepthOnly(const Ref<RenderTarget>& target, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Downsample GBuffer depth only");

            auto pipeline = PipelineManager::GetPipeline(downscaleDepthOnlyPipelineConfig);
            commandList->BindPipeline(pipeline);

            auto rt = target->GetData(RenderResolution::FULL_RES);
            auto downsampledRt = target->GetData(RenderResolution::HALF_RES);

            Downscale(rt, downsampledRt, commandList);

            Graphics::Profiler::EndQuery();

        }

        void GBufferRenderer::FillNormalTexture(const Ref<RenderTarget>& target, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Patch GBuffer normals");

            auto pipeline = PipelineManager::GetPipeline(patchNormalPipelineConfig);
            commandList->BindPipeline(pipeline);

            auto rt = target->GetData(RenderResolution::FULL_RES);
            
            auto normal = rt->normalTexture;
            auto geometryNormal = rt->geometryNormalTexture;
            auto materialIdx = rt->materialIdxTexture;
            
            ivec2 res = ivec2(normal->width, normal->height);

            ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
            groupCount.x += ((res.x % 8 == 0) ? 0 : 1);
            groupCount.y += ((res.y % 8 == 0) ? 0 : 1);

            commandList->BindImage(normal->image, 3, 0);
            commandList->BindImage(geometryNormal->image, geometryNormal->sampler, 3, 1);
            commandList->BindImage(materialIdx->image, materialIdx->sampler, 3, 2);

            commandList->ImageMemoryBarrier(normal->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->ImageMemoryBarrier(normal->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        void GBufferRenderer::GenerateReactiveMask(const Ref<RenderTarget>& target, Graphics::CommandList* commandList) {

            Graphics::Profiler::BeginQuery("Generate reactive mask");

            auto pipeline = PipelineManager::GetPipeline(generateReactiveMaskPipelineConfig);
            commandList->BindPipeline(pipeline);

            auto rt = target->GetData(RenderResolution::FULL_RES);

            auto reactiveMaskTexture = target->reactiveMaskTexture;
            auto stencilTexture = rt->stencilTexture;

            ivec2 res = ivec2(reactiveMaskTexture.width, reactiveMaskTexture.height);

            ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
            groupCount.x += ((res.x % 8 == 0) ? 0 : 1);
            groupCount.y += ((res.y % 8 == 0) ? 0 : 1);

            commandList->BindImage(reactiveMaskTexture.image, 3, 0);
            commandList->BindImage(stencilTexture->image, stencilTexture->sampler, 3, 1);

            commandList->ImageMemoryBarrier(reactiveMaskTexture.image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            commandList->ImageMemoryBarrier(reactiveMaskTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

        }

        void GBufferRenderer::Downscale(const RenderTargetData* rt,
            const RenderTargetData* downsampledRt, Graphics::CommandList* commandList) {

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

            Graphics::ImageBarrier preImageBarriers[] = {
                {depthOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                {normalOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                {geometryNormalOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                {roughnessMetallicAoOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                {velocityOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                {materialIdxOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
                {offsetOut->image, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT}
            };

            commandList->PipelineBarrier(preImageBarriers, {});

            commandList->BindImage(depthOut->image, 3 , 6);
            commandList->BindImage(normalOut->image, 3 , 7);
            commandList->BindImage(geometryNormalOut->image, 3 , 8);
            commandList->BindImage(roughnessMetallicAoOut->image, 3 , 9);
            commandList->BindImage(velocityOut->image, 3 , 10);
            commandList->BindImage(materialIdxOut->image, 3 , 11);
            commandList->BindImage(offsetOut->image, 3 , 12);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::ImageBarrier postImageBarriers[] = {
                {depthOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {normalOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {geometryNormalOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {roughnessMetallicAoOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {velocityOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {materialIdxOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
                {offsetOut->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT}
            };

            commandList->PipelineBarrier(postImageBarriers, {});

        }

    }

}