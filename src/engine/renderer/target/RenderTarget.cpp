#include "RenderTarget.h"
#include "graphics/GraphicsDevice.h"

namespace Atlas::Renderer {

    RenderTarget::RenderTarget(int32_t width, int32_t height) : width(width), height(height) {

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        scaledWidth = int32_t(scalingFactor * width);
        scaledHeight = int32_t(scalingFactor * height);

        ivec2 res = GetRelativeResolution(FULL_RES);
        targetData = RenderTargetData(res, true);

        ivec2 halfRes = GetRelativeResolution(HALF_RES);
        targetDataDownsampled2x = RenderTargetData(halfRes, false);
        targetDataSwapDownsampled2x = RenderTargetData(halfRes, false);

        historyTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        swapHistoryTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        lightingTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        hdrTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        outputTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        oceanDepthTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_D32_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest);
        oceanStencilTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R8_UINT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        {
            Graphics::RenderPassColorAttachment colorAttachments[] = {
                {.imageFormat = targetData.baseColorTexture->format},
                {.imageFormat = targetData.normalTexture->format},
                {.imageFormat = targetData.geometryNormalTexture->format},
                {.imageFormat = targetData.roughnessMetallicAoTexture->format},
                {.imageFormat = targetData.materialIdxTexture->format},
                {.imageFormat = targetData.velocityTexture->format},
                {.imageFormat = targetData.stencilTexture->format},
            };

            for (auto &attachment: colorAttachments) {
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            Graphics::RenderPassDepthAttachment depthAttachment = {
                .imageFormat = targetData.depthTexture->format,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            auto gBufferRenderPassDesc = Graphics::RenderPassDesc{
                .colorAttachments = {colorAttachments[0], colorAttachments[1], colorAttachments[2],
                                     colorAttachments[3], colorAttachments[4], colorAttachments[5], colorAttachments[6]},
                .depthAttachment = depthAttachment
            };
            gBufferRenderPass = graphicsDevice->CreateRenderPass(gBufferRenderPassDesc);
        }
        {
            Graphics::RenderPassColorAttachment colorAttachments[] = {
                {.imageFormat = lightingTexture.format},
                {.imageFormat = targetData.velocityTexture->format},
                {.imageFormat = targetData.stencilTexture->format},
            };

            for (auto &attachment: colorAttachments) {
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
                attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                attachment.outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            Graphics::RenderPassDepthAttachment depthAttachment = {
                .imageFormat = targetData.depthTexture->format,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            auto afterLightingRenderPassDesc = Graphics::RenderPassDesc{
                .colorAttachments = {colorAttachments[0], colorAttachments[1], colorAttachments[2]},
                .depthAttachment = depthAttachment
            };
            afterLightingRenderPass = graphicsDevice->CreateRenderPass(afterLightingRenderPassDesc);
        }
        {
            Graphics::RenderPassColorAttachment colorAttachments[] = {
                {.imageFormat = oceanStencilTexture.format}
            };
            for (auto &attachment: colorAttachments) {
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            Graphics::RenderPassDepthAttachment depthAttachment = {
                .imageFormat = oceanDepthTexture.format,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            auto oceanRenderPassDesc = Graphics::RenderPassDesc {
                .colorAttachments = {colorAttachments[0]},
                .depthAttachment = depthAttachment
            };
            oceanRenderPass = graphicsDevice->CreateRenderPass(oceanRenderPassDesc);
        }

        {
            Graphics::RenderPassColorAttachment colorAttachments[] = {
                {.imageFormat = outputTexture.format}
            };
            for (auto &attachment: colorAttachments) {
                attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment.outputLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            auto outputPassDesc = Graphics::RenderPassDesc {
                .colorAttachments = {colorAttachments[0]}
            };
            outputRenderPass = graphicsDevice->CreateRenderPass(outputPassDesc);
        }

        CreateFrameBuffers();

        SetGIResolution(HALF_RES);
        SetAOResolution(HALF_RES);
        SetVolumetricResolution(HALF_RES);
        SetReflectionResolution(HALF_RES);

        sssTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

    }

    void RenderTarget::Resize(int32_t width, int32_t height) {

        this->width = width;
        this->height = height;

        scaledWidth = int32_t(scalingFactor * width);
        scaledHeight = int32_t(scalingFactor * height);

        targetData.Resize(ivec2(scaledWidth, scaledHeight));

        // We have to also resize the other part of the history
        historyTexture.Resize(scaledWidth, scaledHeight);
        swapHistoryTexture.Resize(scaledWidth, scaledHeight);
        lightingTexture.Resize(scaledWidth, scaledHeight);
        hdrTexture.Resize(width, height);
        outputTexture.Resize(width, height);
        sssTexture.Resize(scaledWidth, scaledHeight);
        oceanDepthTexture.Resize(scaledWidth, scaledHeight);
        oceanStencilTexture.Resize(scaledWidth, scaledHeight);

        SetGIResolution(giResolution);
        SetAOResolution(aoResolution);
        SetVolumetricResolution(volumetricResolution);
        SetReflectionResolution(reflectionResolution);

        ivec2 halfRes = GetRelativeResolution(HALF_RES);
        targetDataDownsampled2x.Resize(halfRes);
        targetDataSwapDownsampled2x.Resize(halfRes);

        CreateFrameBuffers();

        hasHistory = false;

    }

    int32_t RenderTarget::GetWidth() const {

        return width;

    }

    int32_t RenderTarget::GetHeight() const {

        return height;

    }

    int32_t RenderTarget::GetScaledWidth() const {

        return scaledWidth;

    }

    int32_t RenderTarget::GetScaledHeight() const {

        return scaledHeight;

    }

    void RenderTarget::Swap() {

        targetData.velocityTexture.swap(targetData.swapVelocityTexture);

        gBufferFrameBuffer->ChangeColorAttachmentImage(targetData.velocityTexture->image, 5);
        gBufferFrameBuffer->Refresh();

        afterLightingFrameBuffer->ChangeColorAttachmentImage(targetData.velocityTexture->image, 1);
        afterLightingFrameBuffer->Refresh();

        hasHistory = true;
        swap = !swap;

    }

    ivec2 RenderTarget::GetRelativeResolution(RenderResolution resolution) {

        int32_t factor = 1;

        switch (resolution) {
        case HALF_RES: factor = 2; break;
        default: break;
        }

        return ivec2(scaledWidth / factor, scaledHeight / factor);

    }

    void RenderTarget::SetGIResolution(RenderResolution resolution) {

        auto res = GetRelativeResolution(resolution);
        giResolution = resolution;

        giTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        swapGiTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyGiTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        giLengthTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyGiLengthTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

    }

    RenderResolution RenderTarget::GetGIResolution() {

        return giResolution;

    }

    void RenderTarget::SetAOResolution(RenderResolution resolution) {

        auto res = GetRelativeResolution(resolution);
        aoResolution = resolution;

        aoTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        swapAoTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyAoTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        aoLengthTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyAoLengthTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

    }

    RenderResolution RenderTarget::GetAOResolution() {

        return aoResolution;

    }

    void RenderTarget::SetVolumetricResolution(RenderResolution resolution) {

        auto res = GetRelativeResolution(resolution);
        volumetricResolution = resolution;

        volumetricTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT);
        swapVolumetricTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT);

        volumetricCloudsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        swapVolumetricCloudsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyVolumetricCloudsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

    }

    RenderResolution RenderTarget::GetVolumetricResolution() {

        return volumetricResolution;

    }

    void RenderTarget::SetReflectionResolution(RenderResolution resolution) {

        auto res = GetRelativeResolution(resolution);
        reflectionResolution = resolution;

        reflectionTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        swapReflectionTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyReflectionTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

        reflectionMomentsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        historyReflectionMomentsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
    
    }

    RenderResolution RenderTarget::GetReflectionResolution() {

        return reflectionResolution;

    }

    RenderTargetData* RenderTarget::GetData(RenderResolution resolution) {

        switch (resolution) {
        case FULL_RES: return &targetData;
        default: return swap ? &targetDataDownsampled2x : &targetDataSwapDownsampled2x;
        }

    }

    RenderTargetData* RenderTarget::GetHistoryData(RenderResolution resolution) {

        switch (resolution) {
        case FULL_RES: return &targetData; // This is not correct
        default: return swap ? &targetDataSwapDownsampled2x : &targetDataDownsampled2x;
        }

    }

    Texture::Texture2D* RenderTarget::GetHistory() {

        if (swap) {
            return &historyTexture;
        }
        else {
            return &swapHistoryTexture;
        }

    }

    Texture::Texture2D* RenderTarget::GetLastHistory() {

        if (swap) {
            return &swapHistoryTexture;
        }
        else {
            return &historyTexture;
        }

    }

    Texture::Texture2D* RenderTarget::GetVelocity() {

        return targetData.velocityTexture.get();

    }

    Texture::Texture2D* RenderTarget::GetLastVelocity() {

        return targetData.swapVelocityTexture.get();

    }

    bool RenderTarget::HasHistory() const {

        return hasHistory;

    }

    void RenderTarget::SetScalingFactor(float factor) {

        scalingFactor = factor;

        Resize(width, height);

    }

    float RenderTarget::GetScalingFactor() const {

        return scalingFactor;

    }

    void RenderTarget::CreateFrameBuffers() {

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        auto gBufferFrameBufferDesc = Graphics::FrameBufferDesc{
               .renderPass = gBufferRenderPass,
               .colorAttachments = {
                   {targetData.baseColorTexture->image, 0, true},
                   {targetData.normalTexture->image, 0, true},
                   {targetData.geometryNormalTexture->image, 0, true},
                   {targetData.roughnessMetallicAoTexture->image, 0, true},
                   {targetData.materialIdxTexture->image, 0, true},
                   {targetData.velocityTexture->image, 0, true},
                   {targetData.stencilTexture->image, 0, false},
               },
               .depthAttachment = {targetData.depthTexture->image, 0, true},
               .extent = {uint32_t(scaledWidth), uint32_t(scaledHeight)}
        };
        gBufferFrameBuffer = graphicsDevice->CreateFrameBuffer(gBufferFrameBufferDesc);

        auto afterLightingFrameBufferDesc = Graphics::FrameBufferDesc{
               .renderPass = afterLightingRenderPass,
               .colorAttachments = {
                   {lightingTexture.image, 0, true},
                   {targetData.velocityTexture->image, 0, true},
                   {targetData.stencilTexture->image, 0, false}
               },
               .depthAttachment = {targetData.depthTexture->image, 0, true},
               .extent = {uint32_t(scaledWidth), uint32_t(scaledHeight)}
        };
        afterLightingFrameBuffer = graphicsDevice->CreateFrameBuffer(afterLightingFrameBufferDesc);

        afterLightingFrameBufferDesc = Graphics::FrameBufferDesc{
            .renderPass = afterLightingRenderPass,
            .colorAttachments = {
                {lightingTexture.image, 0, true},
                {targetData.velocityTexture->image, 0, true},
                {targetData.stencilTexture->image, 0, true}
            },
            .depthAttachment = {targetData.depthTexture->image, 0, true},
            .extent = {uint32_t(scaledWidth), uint32_t(scaledHeight)}
        };
        afterLightingFrameBufferWithStencil = graphicsDevice->CreateFrameBuffer(afterLightingFrameBufferDesc);

        auto oceanDepthOnlyFrameBufferDesc = Graphics::FrameBufferDesc{
            .renderPass = oceanRenderPass,
            .colorAttachments = {
                {oceanStencilTexture.image, 0, false},
            },
            .depthAttachment = {oceanDepthTexture.image, 0, true},
            .extent = {uint32_t(scaledWidth), uint32_t(scaledHeight)}
        };
        oceanDepthOnlyFrameBuffer = graphicsDevice->CreateFrameBuffer(oceanDepthOnlyFrameBufferDesc);

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