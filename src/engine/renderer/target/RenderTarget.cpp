#include "RenderTarget.h"
#include "graphics/GraphicsDevice.h"

namespace Atlas::Renderer {

    void RenderTargetData::Transition(Graphics::CommandList* commandList, VkImageLayout layout, VkAccessFlags access,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) const {

        Graphics::ImageBarrier imageBarriers[10];
        size_t barrierCount = 0;
        for (auto& texture : { baseColorTexture, depthTexture, normalTexture, geometryNormalTexture,
            roughnessMetallicAoTexture, emissiveTexture, offsetTexture, materialIdxTexture, stencilTexture, velocityTexture }) {
            if (texture)
                imageBarriers[barrierCount++] = { texture->image, layout, access };
        }

        commandList->PipelineBarrier(std::span(imageBarriers, barrierCount), {}, srcStageMask, dstStageMask);

    }

    void RenderTargetDataHistory::Transition(Graphics::CommandList* commandList, VkImageLayout layout, VkAccessFlags access,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) const {

        Graphics::ImageBarrier imageBarriers[7];
        size_t barrierCount = 0;
        for (auto& texture : { depthTexture, normalTexture, geometryNormalTexture, roughnessMetallicAoTexture,
            materialIdxTexture, stencilTexture, velocityTexture }) {
            if (texture)
                imageBarriers[barrierCount++] = { texture->image, layout, access };
        }

        commandList->PipelineBarrier(std::span(imageBarriers, barrierCount), {}, srcStageMask, dstStageMask);

    }

    RenderTarget::RenderTarget(int32_t width, int32_t height) : 
        width(glm::max(4, width)), height(glm::max(4, height)) {

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        // Need to at least have a size of 2x2 pixels
        scaledWidth = glm::max(2, int32_t(scalingFactor * width));
        scaledHeight = glm::max(2, int32_t(scalingFactor * height));

        ivec2 res = GetRelativeResolution(FULL_RES);
        targetData = RenderTargetData(res, true);
        historyData = RenderTargetDataHistory(res, true);

        ivec2 halfRes = GetRelativeResolution(HALF_RES);
        targetDataDownsampled2x = RenderTargetData(halfRes, false);
        historyDataDownsampled2x = RenderTargetDataHistory(halfRes, false);

        historyTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        swapHistoryTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        lightingTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        reactiveMaskTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R8_UNORM,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        hdrTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear, false, true);
        bloomTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapLinear, false, true);
        exposureTexture = Texture::Texture2D(1, 1, VK_FORMAT_R32_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
        outputTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);

        oceanDepthTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_D32_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
        oceanStencilTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R8_UINT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
        oceanNormalTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16G16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);

        CreateRenderings();

        SetGIResolution(HALF_RES, false);
        SetVolumetricResolution(HALF_RES);
        SetReflectionResolution(HALF_RES);

        sssTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

    }

    void RenderTarget::Resize(int32_t width, int32_t height) {

        this->width = glm::max(4, width);
        this->height = glm::max(4, height);

        // Need to at least have a size of 2x2 pixels
        scaledWidth = glm::max(2, int32_t(scalingFactor * width));
        scaledHeight = glm::max(2, int32_t(scalingFactor * height));        

        // We have to also resize the other part of the history
        historyTexture.Resize(scaledWidth, scaledHeight);
        swapHistoryTexture.Resize(scaledWidth, scaledHeight);
        lightingTexture.Resize(scaledWidth, scaledHeight);
        reactiveMaskTexture.Resize(scaledWidth, scaledHeight);
        hdrTexture.Resize(width, height);
        bloomTexture.Resize(width, height);
        outputTexture.Resize(width, height);
        sssTexture.Resize(scaledWidth, scaledHeight);
        oceanDepthTexture.Resize(scaledWidth, scaledHeight);
        oceanStencilTexture.Resize(scaledWidth, scaledHeight);
        oceanNormalTexture.Resize(scaledWidth, scaledHeight);

        SetGIResolution(giResolution, giMomentsTexture.IsValid());
        SetVolumetricResolution(volumetricResolution);
        SetReflectionResolution(reflectionResolution);

        UseForPathTracing(useForPathTracing);

        if (separateReflectionData)
            CreateReflectionData();

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

        hasHistory = true;
        swap = !swap;

        std::swap(historyRadianceTexture, radianceTexture);

        historyData.Swap(targetData);
        if (targetDataDownsampled2x.depthTexture)
            historyDataDownsampled2x.Swap(targetDataDownsampled2x);

        if (separateReflectionData) {
            reflectionHistoryData.Swap(reflectionData);
            reflectionHistoryDataDownsampled2x.Swap(reflectionDataDownsampled2x);
        }

        CreateRenderings();

    }

    ivec2 RenderTarget::GetRelativeResolution(RenderResolution resolution) {

        int32_t factor = 1;

        switch (resolution) {
        case HALF_RES: factor = 2; break;
        default: break;
        }

        return ivec2(scaledWidth / factor, scaledHeight / factor);

    }

    void RenderTarget::SetGIResolution(RenderResolution resolution, bool createMomentsTexture) {

        auto res = GetRelativeResolution(resolution);
        giResolution = resolution;

        giTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        swapGiTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        historyGiTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);

        if (createMomentsTexture) {
            giLengthTexture.Reset();
            historyGiLengthTexture.Reset();
            giMomentsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            historyGiMomentsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        }
        else {
            giMomentsTexture.Reset();
            historyGiMomentsTexture.Reset();
            giLengthTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            historyGiLengthTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        }

        hasHistory = false;

    }

    RenderResolution RenderTarget::GetGIResolution() {

        return giResolution;

    }

    void RenderTarget::SetVolumetricResolution(RenderResolution resolution) {

        auto res = GetRelativeResolution(resolution);
        volumetricResolution = resolution;

        volumetricTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);

        volumetricCloudsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        swapVolumetricCloudsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        historyVolumetricCloudsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);

        hasHistory = false;

    }

    RenderResolution RenderTarget::GetVolumetricResolution() {

        return volumetricResolution;

    }

    void RenderTarget::SetFroxelResolution(ivec3 resolution) {

        froxelTexture = Texture::Texture3D(resolution.x, resolution.y, resolution.z,
            VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        historyFroxelTexture = Texture::Texture3D(resolution.x, resolution.y, resolution.z,
            VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        filteredFroxelTexture = Texture::Texture3D(resolution.x, resolution.y, resolution.z,
            VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        integratedFroxelTexture = Texture::Texture3D(resolution.x, resolution.y, resolution.z,
            VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);

    }

    void RenderTarget::SetReflectionResolution(RenderResolution resolution) {

        auto res = GetRelativeResolution(resolution);
        reflectionResolution = resolution;

        reflectionTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        swapReflectionTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        historyReflectionTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);

        reflectionMomentsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        historyReflectionMomentsTexture = Texture::Texture2D(res.x, res.y, VK_FORMAT_R16G16B16A16_SFLOAT,
            Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);

        hasHistory = false;
    
    }

    RenderResolution RenderTarget::GetReflectionResolution() {

        return reflectionResolution;

    }

    RenderTargetData* RenderTarget::GetData(RenderResolution resolution) {

        switch (resolution) {
        case FULL_RES: return &targetData;
        default: return &targetDataDownsampled2x;
        }

    }

    RenderTargetDataHistory* RenderTarget::GetHistoryData(RenderResolution resolution) {

        switch (resolution) {
        case FULL_RES: return &historyData;
        default: return &historyDataDownsampled2x;
        }

    }

    void RenderTarget::SetSeparateReflectionData(bool separate) {

        if (separate == separateReflectionData)
            return;

        separateReflectionData = separate;

        if (separate) {
            CreateReflectionData();
        }
        else {
            reflectionData = RenderTargetData();
            reflectionDataDownsampled2x = RenderTargetData();
            reflectionHistoryData = RenderTargetDataHistory();
            reflectionHistoryDataDownsampled2x = RenderTargetDataHistory();
        }

        // The history of the reflection data needs to be written first
        hasHistory = false;

    }

    bool RenderTarget::HasSeparateReflectionData() const {

        return separateReflectionData;

    }

    RenderTargetData* RenderTarget::GetReflectionData(RenderResolution resolution) {

        if (!separateReflectionData)
            return GetData(resolution);

        switch (resolution) {
        case FULL_RES: return &reflectionData;
        default: return &reflectionDataDownsampled2x;
        }

    }

    RenderTargetDataHistory* RenderTarget::GetReflectionHistoryData(RenderResolution resolution) {

        if (!separateReflectionData)
            return GetHistoryData(resolution);

        switch (resolution) {
        case FULL_RES: return &reflectionHistoryData;
        default: return &reflectionHistoryDataDownsampled2x;
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

        return GetData(FULL_RES)->velocityTexture.get();

    }

    Texture::Texture2D* RenderTarget::GetLastVelocity() {

        return GetHistoryData(FULL_RES)->velocityTexture.get();

    }

    bool RenderTarget::HasHistory() const {

        return hasHistory;

    }

    void RenderTarget::InvalidateHistory() {

        hasHistory = false;

    }

    void RenderTarget::SetScalingFactor(float factor) {

        scalingFactor = factor;

        Resize(width, height);

    }

    float RenderTarget::GetScalingFactor() const {

        return scalingFactor;

    }

    void RenderTarget::UseForPathTracing(bool use) {

        hasHistory = false;
        useForPathTracing = use;

        if (useForPathTracing) {
            ivec2 res = GetRelativeResolution(FULL_RES);
            targetData = RenderTargetData(res, false);
            historyData = RenderTargetDataHistory(res, false);

            targetDataDownsampled2x = RenderTargetData();
            historyDataDownsampled2x = RenderTargetDataHistory();

            radianceTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R32G32B32A32_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            historyRadianceTexture = Texture::Texture2D(scaledWidth, scaledHeight, VK_FORMAT_R32G32B32A32_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            frameAccumTexture = Texture::Texture2DArray(scaledWidth, scaledHeight, 3, VK_FORMAT_R32_UINT,
                Texture::Wrapping::Repeat, Texture::Filtering::Nearest, false, true);
        }
        else {
            ivec2 res = GetRelativeResolution(FULL_RES);
            targetData = RenderTargetData(res, true);
            historyData = RenderTargetDataHistory(res, true);

            ivec2 halfRes = GetRelativeResolution(HALF_RES);
            targetDataDownsampled2x = RenderTargetData(halfRes, false);
            historyDataDownsampled2x = RenderTargetDataHistory(halfRes, false);

            radianceTexture.Reset();
            historyRadianceTexture.Reset();
            frameAccumTexture.Reset();
        }

        // Pathtracing is kind of tricky with the depth buffer being in a SFLOAT format,
        // so we can't attach this one to render passes
        CreateRenderings();

    }

    bool RenderTarget::IsUsedForPathTracing() const {

        return useForPathTracing;

    }

    void RenderTarget::CreateRenderings() {

        auto target = GetData(FULL_RES);
        auto depthImage = useForPathTracing ? nullptr : target->depthTexture->image;

        auto clear = VK_ATTACHMENT_LOAD_OP_CLEAR;
        gBufferRendering = Graphics::RenderingDesc{
            .colorAttachments = {
                {.image = target->baseColorTexture->image, .loadOp = clear},
                {.image = target->normalTexture->image, .loadOp = clear},
                {.image = target->geometryNormalTexture->image, .loadOp = clear},
                {.image = target->roughnessMetallicAoTexture->image, .loadOp = clear},
                {.image = target->emissiveTexture->image, .loadOp = clear},
                {.image = target->materialIdxTexture->image, .loadOp = clear},
                {.image = target->velocityTexture->image, .loadOp = clear},
                {.image = target->stencilTexture->image, .loadOp = clear, .writeEnabled = false},
            },
            .depthAttachment = {.image = depthImage, .loadOp = clear}
        };

        gBufferLoadRendering = gBufferRendering;
        for (auto& attachment : gBufferLoadRendering.colorAttachments)
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        gBufferLoadRendering.depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        afterLightingRendering = Graphics::RenderingDesc{
            .colorAttachments = {
                {.image = lightingTexture.image},
                {.image = target->velocityTexture->image},
                {.image = target->stencilTexture->image},
            },
            .depthAttachment = {.image = depthImage}
        };

        oceanDepthRendering = Graphics::RenderingDesc{
            .colorAttachments = {
                {.image = oceanStencilTexture.image, .loadOp = clear},
                {.image = oceanNormalTexture.image, .loadOp = clear},
            },
            .depthAttachment = {.image = oceanDepthTexture.image, .loadOp = clear}
        };

        outputRendering = Graphics::RenderingDesc{
            .colorAttachments = {
                {.image = outputTexture.image, .loadOp = clear}
            }
        };

    }

    void RenderTarget::CreateReflectionData() {

        // The data is written by compute shaders, so the depth can't be in a depth format
        ivec2 res = GetRelativeResolution(FULL_RES);
        reflectionData = RenderTargetData(res, false, true);
        reflectionHistoryData = RenderTargetDataHistory(res, false);

        ivec2 halfRes = GetRelativeResolution(HALF_RES);
        reflectionDataDownsampled2x = RenderTargetData(halfRes, false, true);
        reflectionHistoryDataDownsampled2x = RenderTargetDataHistory(halfRes, false);

    }

}