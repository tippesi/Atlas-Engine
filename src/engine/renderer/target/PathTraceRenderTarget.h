#pragma once

#include "System.h"
#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "graphics/RenderPass.h"
#include "graphics/Framebuffer.h"

namespace Atlas::Renderer {

    class PathTracerRenderTarget {

    public:
        PathTracerRenderTarget() {}

        PathTracerRenderTarget(int32_t width, int32_t height);

        void Resize(int32_t width, int32_t height);

        void Swap();

        int32_t GetWidth() const { return width; }
        int32_t GetHeight() const { return height; }

        Texture::Texture2D texture;

        Texture::Texture2DArray frameAccumTexture;

        Texture::Texture2D albedoTexture;
        Texture::Texture2D velocityTexture;
        Texture::Texture2D depthTexture;
        Texture::Texture2D normalTexture;
        Texture::Texture2D materialIdxTexture;

        Texture::Texture2D historyVelocityTexture;
        Texture::Texture2D historyDepthTexture;
        Texture::Texture2D historyNormalTexture;
        Texture::Texture2D historyMaterialIdxTexture;

        Texture::Texture2D radianceTexture;
        Texture::Texture2D historyRadianceTexture;

        Texture::Texture2D postProcessTexture;
        Texture::Texture2D historyPostProcessTexture;

        Texture::Texture2D outputTexture;

        Ref<Graphics::RenderPass> outputRenderPass;
        Ref<Graphics::FrameBuffer> outputFrameBuffer;

        int32_t sampleCount = 0;

    private:
        void CreateFramebuffers();

        int32_t width = 0;
        int32_t height = 0;

    };

}