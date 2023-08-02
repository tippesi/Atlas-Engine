#ifndef AE_OCEANRENDERER_H
#define AE_OCEANRENDERER_H

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

        private:
            struct alignas(16) Uniforms {
                vec4 translation;

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

                vec4 terrainTranslation;

                float terrainHeightScale;
                int32_t N;
            };

            struct alignas(16) Cascade {
                float distance;
                float texelSize;
                float aligment0;
                float aligment1;
                mat4 cascadeSpace;
            };

            struct alignas(16) Shadow {
                float distance;
                float bias;

                float cascadeBlendDistance;

                int cascadeCount;
                vec2 resolution;

                Cascade cascades[6];
            };

            struct alignas(16) Light {
                vec4 location;
                vec4 direction;

                vec4 color;
                float intensity;

                float scatteringFactor;

                float radius;

                Shadow shadow;
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

            PipelineConfig GeneratePipelineConfig(RenderTarget* target, bool wireframe);

            PipelineConfig causticPipelineConfig;

            Buffer::VertexArray vertexArray;

            Texture::Texture2D refractionTexture;
            Texture::Texture2D depthTexture;

            Buffer::UniformBuffer uniformBuffer;
            Buffer::UniformBuffer lightUniformBuffer;

            Ref<Graphics::Sampler> nearestSampler;
            Ref<Graphics::Sampler> shadowSampler;

        };

    }

}


#endif