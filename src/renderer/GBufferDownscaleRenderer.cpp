#include "GBufferDownscaleRenderer.h"

namespace Atlas {

    namespace Renderer {

        GBufferDownscaleRenderer::GBufferDownscaleRenderer() {

            downscale.AddStage(AE_COMPUTE_STAGE, "downsampleGBuffer2x.csh");
            downscale.Compile();

            downscaleDepthOnly.AddStage(AE_COMPUTE_STAGE, "downsampleGBuffer2x.csh");
            downscaleDepthOnly.AddMacro("DEPTH_ONLY");
            downscaleDepthOnly.Compile();

        }

        void GBufferDownscaleRenderer::Render(Viewport *viewport, RenderTarget *target, 
            Camera *camera, Scene::Scene *scene) {


            
        }

        void GBufferDownscaleRenderer::Downscale(RenderTarget* target) {

            Profiler::BeginQuery("Downsample GBuffer");

            downscale.Bind();

            auto depthIn = target->GetDownsampledDepthTexture(RenderResolution::FULL_RES);
            auto normalIn = target->GetDownsampledNormalTexture(RenderResolution::FULL_RES);
            auto roughnessMetallicAoIn = target->GetDownsampledRoughnessMetalnessAoTexture(RenderResolution::FULL_RES);
            auto depthOut = target->GetDownsampledDepthTexture(RenderResolution::HALF_RES);
            auto normalOut = target->GetDownsampledNormalTexture(RenderResolution::HALF_RES);
            auto roughnessMetallicAoOut = target->GetDownsampledRoughnessMetalnessAoTexture(RenderResolution::HALF_RES);
            auto offsetOut = target->GetDownsampledOffsetTexture(RenderResolution::HALF_RES);

            Downscale(depthIn, normalIn, roughnessMetallicAoIn, depthOut, normalOut,
                      roughnessMetallicAoOut, offsetOut);

            Profiler::EndQuery();

        }

        void GBufferDownscaleRenderer::DownscaleDepthOnly(RenderTarget* target) {

            Profiler::BeginQuery("Downsample GBuffer depth only");

            downscaleDepthOnly.Bind();

            auto depthIn = target->GetDownsampledDepthTexture(RenderResolution::FULL_RES);
            auto normalIn = target->GetDownsampledNormalTexture(RenderResolution::FULL_RES);
            auto roughnessMetallicAoIn = target->GetDownsampledRoughnessMetalnessAoTexture(RenderResolution::FULL_RES);
            auto depthOut = target->GetDownsampledDepthTexture(RenderResolution::HALF_RES);
            auto normalOut = target->GetDownsampledNormalTexture(RenderResolution::HALF_RES);
            auto roughnessMetallicAoOut = target->GetDownsampledRoughnessMetalnessAoTexture(RenderResolution::HALF_RES);
            auto offsetOut = target->GetDownsampledOffsetTexture(RenderResolution::HALF_RES);

            Downscale(depthIn, normalIn, roughnessMetallicAoIn, depthOut, normalOut,
                      roughnessMetallicAoOut, offsetOut);

            Profiler::EndQuery();

        }

        void GBufferDownscaleRenderer::Downscale(Texture::Texture2D* depthIn, Texture::Texture2D* normalIn,
            Texture::Texture2D* roughnessMetallicAoIn, Texture::Texture2D* depthOut, 
            Texture::Texture2D* normalOut, Texture::Texture2D* roughnessMetallicAoOut,
            Texture::Texture2D* offsetOut) {

            ivec2 res = ivec2(depthOut->width, depthOut->height);

            ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
            groupCount.x += ((res.x % 8 == 0) ? 0 : 1);
            groupCount.y += ((res.y % 8 == 0) ? 0 : 1);

            depthIn->Bind(GL_TEXTURE0);
            normalIn->Bind(GL_TEXTURE1);
            roughnessMetallicAoIn->Bind(GL_TEXTURE2);

            depthOut->Bind(GL_WRITE_ONLY, 3);
            normalOut->Bind(GL_WRITE_ONLY, 4);
            roughnessMetallicAoOut->Bind(GL_WRITE_ONLY, 5);
            offsetOut->Bind(GL_WRITE_ONLY, 6);

            glDispatchCompute(groupCount.x, groupCount.y, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        }

    }

}