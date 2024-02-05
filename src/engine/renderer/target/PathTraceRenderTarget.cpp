#include "PathTraceRenderTarget.h"

namespace Atlas::Renderer {

    PathTracerRenderTarget::PathTracerRenderTarget(int32_t width, int32_t height) : width(width), height(height) {
        
        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;
        
        texture = Texture::Texture2D(width, height, VK_FORMAT_R8G8B8A8_UNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        frameAccumTexture = Texture::Texture2DArray(width, height, 3, VK_FORMAT_R32_UINT);

        albedoTexture = Texture::Texture2D(width, height, VK_FORMAT_R8G8B8A8_UNORM);

        velocityTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16_SFLOAT);
        historyVelocityTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16_SFLOAT);

        depthTexture = Texture::Texture2D(width, height, VK_FORMAT_R32_SFLOAT);
        historyDepthTexture = Texture::Texture2D(width, height, VK_FORMAT_R32_SFLOAT);

        normalTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16_SFLOAT);
        historyNormalTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16_SFLOAT);

        materialIdxTexture = Texture::Texture2D(width, height, VK_FORMAT_R16_UINT);
        historyMaterialIdxTexture = Texture::Texture2D(width, height, VK_FORMAT_R16_UINT);

        radianceTexture = Texture::Texture2D(width, height, VK_FORMAT_R32G32B32A32_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyRadianceTexture = Texture::Texture2D(width, height, VK_FORMAT_R32G32B32A32_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        postProcessTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyPostProcessTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        outputTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        {
            Graphics::RenderPassColorAttachment colorAttachments[] = {
                {.imageFormat = outputTexture.format}
            };
            for (auto& attachment : colorAttachments) {
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            auto outputPassDesc = Graphics::RenderPassDesc{
                .colorAttachments = {colorAttachments[0]}
            };
            outputRenderPass = graphicsDevice->CreateRenderPass(outputPassDesc);
        }

        CreateFramebuffers();

    }

    void PathTracerRenderTarget::Resize(int32_t width, int32_t height) {
        this->width = width;
        this->height = height;

        texture.Resize(width, height);

        frameAccumTexture.Resize(width, height, 3);

        albedoTexture.Resize(width, height);

        velocityTexture.Resize(width, height);
        historyVelocityTexture.Resize(width, height);

        depthTexture.Resize(width, height);
        historyDepthTexture.Resize(width, height);

        normalTexture.Resize(width, height);
        historyNormalTexture.Resize(width, height);

        materialIdxTexture.Resize(width, height);
        historyMaterialIdxTexture.Resize(width, height);

        radianceTexture.Resize(width, height);
        historyRadianceTexture.Resize(width, height);

        postProcessTexture.Resize(width, height);
        historyPostProcessTexture.Resize(width, height);

        outputTexture.Resize(width, height);

        CreateFramebuffers();

    }

    void PathTracerRenderTarget::Swap() {

        std::swap(radianceTexture, historyRadianceTexture);
        std::swap(velocityTexture, historyVelocityTexture);
        std::swap(depthTexture, historyDepthTexture);
        std::swap(normalTexture, historyNormalTexture);
        std::swap(materialIdxTexture, historyMaterialIdxTexture);
        std::swap(postProcessTexture, historyPostProcessTexture);

    }

    void PathTracerRenderTarget::CreateFramebuffers() {

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        auto outputFrameBufferDesc = Graphics::FrameBufferDesc{
           .renderPass = outputRenderPass,
           .colorAttachments = {
               {outputTexture.image, 0, true}
           },
           .extent = {uint32_t(width), uint32_t(height)}
        };
        outputFrameBuffer = graphicsDevice->CreateFrameBuffer(outputFrameBufferDesc);

    }

}