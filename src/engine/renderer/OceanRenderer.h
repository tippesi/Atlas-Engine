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
            Buffer::UniformBuffer cloudShadowUniformBuffer;

            Ref<Graphics::Sampler> nearestSampler;
            Ref<Graphics::Sampler> shadowSampler;

        };

    }

}


#endif