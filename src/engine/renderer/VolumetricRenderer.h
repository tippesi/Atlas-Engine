#pragma once

#include "../System.h"
#include "../Filter.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class VolumetricRenderer : public Renderer {

        public:
            VolumetricRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) CullingPushConstants {
                int32_t lightCount;
            };

            struct alignas(16) FroxelPushConstants {
                ivec2 columnCount;
                ivec2 resolution;
                int32_t sliceCount;
                int32_t tileSize;
                float near;
                float far;
                int32_t lightCount;
                int32_t resetHistory;
                int32_t padding0;
                int32_t padding1;
                vec4 jitter;
                float extendedFar;
                // Temporary debug options
                float currentWeight;
                float distributionExponent;
                int32_t directionalJitter;
                int32_t localLightJitter;
                int32_t localLightSampleCount;
                int32_t jitterMode;
            };

            struct alignas(16) VolumetricUniforms {
                int sampleCount;
                float intensity;
                int fogEnabled;
                float oceanHeight;
                int lightCount;
                int offsetX;
                int offsetY;
                int directionalLightCount;
                vec4 planetCenterAndRadius;
                Fog fog;
                CloudShadow cloudShadow;
            };

            struct alignas(16) BlurConstants {

            };

            struct alignas(16) ResolveUniforms {
                Fog fog;
                vec4 planetCenter;
                vec4 mainLightColor;
                vec4 mainLightDirection;
                int downsampled2x;
                int cloudsEnabled;
                int fogEnabled;
                float innerCloudRadius;
                float planetRadius;
                float cloudDistanceLimit;
            };

            Filter blurFilter;

            void RenderFroxels(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, ivec2 resolution,
                int32_t lightCount, bool cloudsEnabled, bool cloudShadowsEnabled, bool oceanEnabled,
                Graphics::CommandList* commandList);

            PipelineConfig volumetricPipelineConfig;

            PipelineConfig froxelCullingPipelineConfig;
            PipelineConfig froxelInjectPipelineConfig;
            PipelineConfig froxelIntegratePipelineConfig;
            PipelineConfig froxelApplyPipelineConfig;

            uint64_t froxelFrameCount = 0;

            // Has to match FROXEL_CLUSTER_SIZE
            const int32_t froxelClusterSize = 4;

            PipelineConfig horizontalBlurPipelineConfig;
            PipelineConfig verticalBlurPipelineConfig;

            PipelineConfig resolvePipelineConfig;

            Texture::Texture2D scramblingRankingTexture;
            Texture::Texture2D sobolSequenceTexture;

            Buffer::Buffer lightCullingBuffer;
            Buffer::Buffer blurWeightsUniformBuffer;
            Buffer::UniformBuffer volumetricUniformBuffer;
            Buffer::UniformBuffer resolveUniformBuffer;
            
            Ref<Graphics::Sampler> shadowSampler;

            const int32_t filterSize = 4;

        };

    }

}