#pragma once

#include "System.h"
#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "texture/Texture3D.h"
#include "graphics/RenderPass.h"
#include "graphics/Framebuffer.h"
#include "graphics/CommandList.h"

namespace Atlas::Renderer {

    enum RenderResolution {
        FULL_RES = 0,
        HALF_RES
    };

    class RenderTargetData {
    public:
        RenderTargetData() = default;

        // The reflection data only has the channels the reflections read
        explicit RenderTargetData(ivec2 resolution, bool useDepthFormat, bool reflectionData = false) {
            if (!reflectionData) {
                baseColorTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                    VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
                emissiveTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                    VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
                stencilTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R8_UINT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);

                auto hiZPadding = 1 << (hiZLevelCount - 1);
                auto hiZResolution = ((resolution + hiZPadding - 1) / hiZPadding) * hiZPadding;
                hiZTexture = std::make_shared<Texture::Texture2D>(hiZResolution.x, hiZResolution.y,
                    VK_FORMAT_R32G32_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::MipMapNearest, false, true);
            }
            if (useDepthFormat) {
                depthTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_D32_SFLOAT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            }
            else {
                depthTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R32_SFLOAT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            }
            normalTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            geometryNormalTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            roughnessMetallicAoTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            offsetTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R8_SINT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            materialIdxTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R16_UINT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            velocityTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        }

        // Transitions all existing channels, except the hierarchical depth which has its own layouts
        void Transition(Graphics::CommandList* commandList, VkImageLayout layout, VkAccessFlags access,
            VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) const;

        Ref<Texture::Texture2D> baseColorTexture = nullptr;
        Ref<Texture::Texture2D> depthTexture = nullptr;
        Ref<Texture::Texture2D> normalTexture = nullptr;
        Ref<Texture::Texture2D> geometryNormalTexture = nullptr;
        Ref<Texture::Texture2D> roughnessMetallicAoTexture = nullptr;
        Ref<Texture::Texture2D> emissiveTexture = nullptr;
        Ref<Texture::Texture2D> offsetTexture = nullptr;
        Ref<Texture::Texture2D> materialIdxTexture = nullptr;
        Ref<Texture::Texture2D> stencilTexture = nullptr;
        Ref<Texture::Texture2D> velocityTexture = nullptr;
        Ref<Texture::Texture2D> hiZTexture = nullptr;

        int32_t hiZLevelCount = 8;
    };

    // Only the channels of the last frame that the temporal passes read, swapped with the current data
    class RenderTargetDataHistory {
    public:
        RenderTargetDataHistory() = default;

        explicit RenderTargetDataHistory(ivec2 resolution, bool useDepthFormat) {
            if (useDepthFormat) {
                depthTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_D32_SFLOAT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            }
            else {
                depthTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R32_SFLOAT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            }
            normalTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            geometryNormalTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            roughnessMetallicAoTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
            materialIdxTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R16_UINT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            stencilTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R8_UINT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest, false, true);
            velocityTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear, false, true);
        }

        void Transition(Graphics::CommandList* commandList, VkImageLayout layout, VkAccessFlags access,
            VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) const;

        void Swap(RenderTargetData& data) {
            std::swap(depthTexture, data.depthTexture);
            std::swap(normalTexture, data.normalTexture);
            std::swap(geometryNormalTexture, data.geometryNormalTexture);
            std::swap(roughnessMetallicAoTexture, data.roughnessMetallicAoTexture);
            std::swap(materialIdxTexture, data.materialIdxTexture);
            std::swap(velocityTexture, data.velocityTexture);
            // The reflection data has no stencil
            if (data.stencilTexture)
                std::swap(stencilTexture, data.stencilTexture);
        }

        Ref<Texture::Texture2D> depthTexture = nullptr;
        Ref<Texture::Texture2D> normalTexture = nullptr;
        Ref<Texture::Texture2D> geometryNormalTexture = nullptr;
        Ref<Texture::Texture2D> roughnessMetallicAoTexture = nullptr;
        Ref<Texture::Texture2D> materialIdxTexture = nullptr;
        Ref<Texture::Texture2D> stencilTexture = nullptr;
        Ref<Texture::Texture2D> velocityTexture = nullptr;
    };

    class RenderTarget {

    public:
        /**
         * Constructs a RenderTarget object.
         */
        RenderTarget() = default;

        /**
         * Constructs a RenderTarget object.
         * @param width The width of the render target
         * @param height The height of the render target
         */
        explicit RenderTarget(int32_t width, int32_t height);

        /**
         * Resizes the render target.
         * @param width The width of the render target
         * @param height The height of the render target
         */
        void Resize(int32_t width, int32_t height);

        /**
         * Returns the width of the render target.
         * @return The width of the render target.
         */
        int32_t GetWidth() const;

        /**
         * Returns the width of the render target.
         * @return The width of the render target.
         */
        int32_t GetHeight() const;

        /**
         * Returns the width of the render target.
         * @return The width of the render target.
         */
        int32_t GetScaledWidth() const;

        /**
         * Returns the width of the render target.
         * @return The width of the render target.
         */
        int32_t GetScaledHeight() const;

        /**
         * Swaps the history textures.
         * @note This is used internally for TAA.
         */
        void Swap();

        /**
         * Returns the relative resolution based on the render target resolution
         * @param resolution The resolution "factor" for the relative resolution
         */
        ivec2 GetRelativeResolution(RenderResolution resolution);

        /*
         * Sets the render resolution for screen space global illumination
         */
        void SetGIResolution(RenderResolution resolution, bool createMomentsTexture);

        /*
         * Gets the render resolution for screen space global illumination
         */
        RenderResolution GetGIResolution();

        /*
         * Sets the render resolution for volumetric effetcs
         */
        void SetVolumetricResolution(RenderResolution resolution);

        /*
         * Gets the render resolution for volumetric effetcs
         */
        RenderResolution GetVolumetricResolution();

        /*
         * Sets the resolution of the froxel grid of the volumetric fog
         */
        void SetFroxelResolution(ivec3 resolution);

        /*
         * Sets the render resolution for reflections
         */
        void SetReflectionResolution(RenderResolution resolution);

        /*
         * Gets the render resolution for reflections
         */
        RenderResolution GetReflectionResolution();

        RenderTargetData* GetData(RenderResolution resolution);

        RenderTargetDataHistory* GetHistoryData(RenderResolution resolution);

        /*
         * Gives the reflections their own geometry data, e.g. for the ocean surface
         */
        void SetSeparateReflectionData(bool separate);

        bool HasSeparateReflectionData() const;

        RenderTargetData* GetReflectionData(RenderResolution resolution);

        RenderTargetDataHistory* GetReflectionHistoryData(RenderResolution resolution);

        Texture::Texture2D* GetHistory();

        Texture::Texture2D* GetLastHistory();

        Texture::Texture2D* GetVelocity();

        Texture::Texture2D* GetLastVelocity();

        bool HasHistory() const;

        /**
         * Temporal effects (anti-aliasing, exposure, ...) start over in the next frame, e.g. after a camera cut.
         */
        void InvalidateHistory();

        void SetScalingFactor(float factor);

        float GetScalingFactor() const;

        void UseForPathTracing(bool use);

        bool IsUsedForPathTracing() const;

        Ref<Graphics::RenderPass> gBufferRenderPass;
        Ref<Graphics::FrameBuffer> gBufferFrameBuffer;

        Ref<Graphics::RenderPass> afterLightingRenderPass;
        Ref<Graphics::FrameBuffer> afterLightingFrameBuffer;
        Ref<Graphics::FrameBuffer> afterLightingFrameBufferWithStencil;

        Ref<Graphics::RenderPass> oceanRenderPass;
        Ref<Graphics::FrameBuffer> oceanDepthOnlyFrameBuffer;

        Ref<Graphics::RenderPass> outputRenderPass;
        Ref<Graphics::FrameBuffer> outputFrameBuffer;

        Texture::Texture2D outputTexture;
        Texture::Texture2D bloomTexture;
        Texture::Texture2D exposureTexture;

        Texture::Texture2D giTexture;
        Texture::Texture2D swapGiTexture;
        Texture::Texture2D historyGiTexture;
        Texture::Texture2D giLengthTexture;
        Texture::Texture2D historyGiLengthTexture;
        Texture::Texture2D giMomentsTexture;
        Texture::Texture2D historyGiMomentsTexture;

        Texture::Texture2D sssTexture;

        Texture::Texture2D oceanDepthTexture;
        Texture::Texture2D oceanStencilTexture;
        Texture::Texture2D oceanNormalTexture;

        Texture::Texture2D volumetricTexture;
        Texture::Texture2D swapVolumetricTexture;

        Texture::Texture2D volumetricCloudsTexture;
        Texture::Texture2D swapVolumetricCloudsTexture;
        Texture::Texture2D historyVolumetricCloudsTexture;

        Texture::Texture3D froxelTexture;
        Texture::Texture3D historyFroxelTexture;
        Texture::Texture3D filteredFroxelTexture;
        Texture::Texture3D integratedFroxelTexture;

        Texture::Texture2D reflectionTexture;
        Texture::Texture2D swapReflectionTexture;
        Texture::Texture2D historyReflectionTexture;
        Texture::Texture2D reflectionMomentsTexture;
        Texture::Texture2D historyReflectionMomentsTexture;

        Texture::Texture2D radianceTexture;
        Texture::Texture2D historyRadianceTexture;
        Texture::Texture2DArray frameAccumTexture;

        Texture::Texture2D lightingTexture;
        Texture::Texture2D reactiveMaskTexture;
        Texture::Texture2D hdrTexture;

        int32_t sampleCount = 0;

    private:
        void CreateRenderPasses();

        void CreateFrameBuffers();

        void CreateReflectionData();

        Texture::Texture2D historyTexture;
        Texture::Texture2D swapHistoryTexture;

        RenderTargetData targetData;
        RenderTargetData targetDataDownsampled2x;
        RenderTargetDataHistory historyData;
        RenderTargetDataHistory historyDataDownsampled2x;

        RenderTargetData reflectionData;
        RenderTargetData reflectionDataDownsampled2x;
        RenderTargetDataHistory reflectionHistoryData;
        RenderTargetDataHistory reflectionHistoryDataDownsampled2x;

        int32_t width = 0;
        int32_t height = 0;

        int32_t scaledHeight = 0;
        int32_t scaledWidth = 0;

        RenderResolution giResolution;
        RenderResolution volumetricResolution;
        RenderResolution reflectionResolution;

        float scalingFactor = 0.75f;

        bool swap = false;
        bool hasHistory = false;
        bool useForPathTracing = false;
        bool separateReflectionData = false;

    };

}