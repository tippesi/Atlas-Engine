#pragma once

#include "../System.h"
#include "Renderer.h"

#include "../ocean/OceanSimulation.h"

namespace Atlas {

    namespace Renderer {

        class OceanRenderer : public Renderer {

        public:
            OceanRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

            void RenderDepthOnly(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

        private:
            struct alignas(16) Uniforms {
                Fog fog;

                vec4 waterBodyColor;
                vec4 deepWaterBodyColor;
                vec4 scatterColor;

                vec4 translation;
                vec4 terrainTranslation;

                vec4 waterColorIntensity;

                vec4 spectrumTilingFactors;
                vec4 spectrumWeights;
                vec4 spectrumFadeoutDistances;

                float displacementScale;
                float choppyScale;
                float tiling;
                int hasRippleTexture;

                float shoreWaveDistanceOffset;
                float shoreWaveDistanceScale;
                float shoreWaveAmplitude;
                float shoreWaveSteepness;

                float shoreWavePower;
                float shoreWaveSpeed;
                float shoreWaveLength;
                float terrainSideLength;

                float terrainHeightScale;
                int32_t N;
                int32_t spectrumCount;
                float innerCloudRadius;
            };

            struct alignas(16) PushConstants {
                float nodeSideLength;
                float tileScale;
                float patchSize;
                float heightScale;

                float leftLoD;
                float topLoD;
                float rightLoD;
                float bottomLoD;

                vec2 nodeLocation;
            };

            PipelineConfig GeneratePipelineConfig(RenderTarget* target, bool depthOnly, bool wireframe);

            PipelineConfig causticPipelineConfig;
            PipelineConfig underWaterPipelineConfig;

            Buffer::VertexArray vertexArray;

            Texture::Texture2D refractionTexture;
            Texture::Texture2D depthTexture;
            Texture::Texture2D dummyTexture;

            Buffer::UniformBuffer uniformBuffer;
            Buffer::UniformBuffer depthUniformBuffer;
            Buffer::UniformBuffer lightUniformBuffer;
            Buffer::UniformBuffer cloudShadowUniformBuffer;

            Ref<Graphics::Sampler> nearestSampler;
            Ref<Graphics::Sampler> shadowSampler;

        };

    }

}