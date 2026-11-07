#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class VolumetricRenderer : public Renderer {

        public:
            VolumetricRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) FroxelPushConstants {
                ivec2 columnCount;
                ivec2 resolution;
                int32_t sliceCount;
                int32_t tileSize;
                float near;
                float far;
                int32_t lightCount;
                int32_t resetHistory;
                int32_t fogVolumeCount;
                int32_t padding0;
                vec4 jitter;
                float extendedFar;
                float distributionExponent;
                float padding1;
                float padding2;
            };

            struct alignas(16) VolumetricUniforms {
                float intensity;
                int directionalLightCount;
                int padding0;
                int padding1;
                vec4 planetCenterAndRadius;
                Fog fog;
                CloudShadow cloudShadow;
            };

            struct alignas(16) ResolveUniforms {
                vec4 planetCenter;
                int downsampled2x;
                int cloudsEnabled;
                float innerCloudRadius;
                float planetRadius;
                float cloudDistanceLimit;
            };

            void RenderFroxels(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene,
                Graphics::CommandList* commandList);

            PipelineConfig froxelCullingPipelineConfig;
            PipelineConfig froxelInjectPipelineConfig;
            PipelineConfig froxelHorizontalFilterPipelineConfig;
            PipelineConfig froxelVerticalFilterPipelineConfig;
            PipelineConfig froxelIntegratePipelineConfig;
            PipelineConfig froxelApplyPipelineConfig;

            uint64_t froxelFrameCount = 0;

            // Has to match FROXEL_CLUSTER_SIZE
            const int32_t froxelClusterSize = 4;

            PipelineConfig resolvePipelineConfig;

            Buffer::Buffer lightCullingBuffer;
            Buffer::Buffer fogVolumeBuffer;
            Buffer::Buffer fogVolumeMaskBuffer;
            Buffer::UniformBuffer volumetricUniformBuffer;
            Buffer::UniformBuffer resolveUniformBuffer;

            Ref<Graphics::Sampler> shadowSampler;
            Ref<Graphics::Sampler> shadowDepthSampler;

        };

    }

}
