#pragma once

#include "Renderer.h"

#include "texture/Texture3D.h"

namespace Atlas {

    namespace Renderer {

        class VolumetricCloudRenderer : public Renderer {

        public:
            VolumetricCloudRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            void RenderShadow(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            void GenerateTextures(Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) CloudUniforms {
                Light light;

                float planetRadius;
                float innerRadius;
                float outerRadius;
                float distanceLimit;

                float coverageScale;
                float shapeScale;
                float verticalShapeScale;
                float detailScale;
                float detailStrength;
                float densityMultiplier;

                float coverage;
                float cloudType;
                float bottomType;
                float heightVariation;
                float anvilBias;
                float shapeErosion;

                float extinctionFactor;
                float scatteringFactor;
                float eccentricityFirstPhase;
                float eccentricitySecondPhase;
                float phaseAlpha;

                float time;
                uint32_t frameSeed;

                int32_t sampleCount;
                int32_t occlusionSampleCount;
                int32_t padding0;
                int32_t padding1;
                int32_t padding2;

                vec4 extinctionCoefficients;
                vec4 planetCenter;
                // xy: wind velocity, zw: offset of the weather map
                vec4 windAndCoverageOffset;
            };

            struct CloudTemporalPushConstants {
                int32_t resetHistory;
                int32_t downsampled2x;
            };

            struct alignas(16) CloudShadowUniforms {
                mat4 ivMatrix;
                mat4 ipMatrix;
                vec4 lightDirection;
                int32_t shadowSampleFraction;
            };

            void GenerateShapeTexture(Graphics::CommandList* commandList,
                Texture::Texture3D* texture, float seed);
            
            void GenerateDetailTexture(Graphics::CommandList* commandList,
                Texture::Texture3D* texture, float seed);

            CloudUniforms GetUniformStructure(Ref<Scene::Scene> scene,
                Scene::Entity mainLightEntity);

            PipelineConfig shapeNoisePipelineConfig;
            PipelineConfig detailNoisePipelineConfig;

            PipelineConfig integratePipelineConfig;
            PipelineConfig temporalPipelineConfig;

            PipelineConfig shadowPipelineConfig;

            Texture::Texture2D scramblingRankingTexture;
            Texture::Texture2D sobolSequenceTexture;
            Buffer::UniformBuffer volumetricUniformBuffer;

            Buffer::UniformBuffer shadowUniformBuffer;
            Buffer::UniformBuffer shadowVolumetricUniformBuffer;

            uint32_t frameCount = 0;

        };

    }

}
