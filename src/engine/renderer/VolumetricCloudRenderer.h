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

                float heightStretch;

                float coverageScale;
                float shapeScale;
                float detailScale;
                float coverageSpeed;
                float shapeSpeed;
                float detailSpeed;
                float detailStrength;

                float extinctionFactor;
                float scatteringFactor;

                float eccentricityFirstPhase;
                float eccentricitySecondPhase;
                float phaseAlpha;

                float densityMultiplier;

                float time;
                uint32_t frameSeed;

                int32_t sampleCount;
                int32_t occlusionSampleCount;

                float darkEdgeDirect;
                float darkEdgeDetail;

                vec4 extinctionCoefficients;
                vec4 planetCenter;
            };

            struct alignas(16) CloudTemporalPushConstants {
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
                Texture::Texture3D* texture, float baseScale);
            
            void GenerateDetailTexture(Graphics::CommandList* commandList,
                Texture::Texture3D* texture, float baseScale);

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