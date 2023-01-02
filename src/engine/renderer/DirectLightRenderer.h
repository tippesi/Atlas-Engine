#ifndef AE_DIRECTLIGHTRENDERER_H
#define AE_DIRECTLIGHTRENDERER_H

#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class DirectLightRenderer : public Renderer {

		public:
			DirectLightRenderer() = default;

			void Init(GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
				Scene::Scene* scene, CommandList* commandList);

		private:
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

            Ref<MultiBuffer> uniformBuffer;
            Ref<Pipeline> pipeline;
            Ref<Sampler> shadowSampler;

		};

	}

}

#endif
