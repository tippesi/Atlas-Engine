#ifndef AE_VOLUMETRICCLOUDRENDERER_H
#define AE_VOLUMETRICCLOUDRENDERER_H

#include "Renderer.h"

#include "texture/Texture3D.h"

namespace Atlas {

	namespace Renderer {

		class VolumetricCloudRenderer : public Renderer {

		public:
			VolumetricCloudRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene) final {};

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

			void GenerateTextures(Scene::Scene* scene, Graphics::CommandList* commandList);

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

			struct alignas(16) VolumetricCloudUniforms {
                Light light;

                float planetRadius;
                float innerRadius;
                float outerRadius;
                float distanceLimit;

                float lowerHeightFalloff;
                float upperHeightFalloff;

                float shapeScale;
                float detailScale;
                float shapeSpeed;
                float detailSpeed;
                float detailStrength;

                float eccentricity;
                float extinctionFactor;
                float scatteringFactor;

                float silverLiningSpread;
                float silverLiningIntensity;

                float densityMultiplier;

                float time;
                uint32_t frameSeed;
			};

			void GenerateShapeTexture(Graphics::CommandList* commandList,
                Texture::Texture3D* texture, float baseScale);
			
			void GenerateDetailTexture(Graphics::CommandList* commandList,
                Texture::Texture3D* texture, float baseScale);

            PipelineConfig shapeNoisePipelineConfig;
            PipelineConfig detailNoisePipelineConfig;

            PipelineConfig integratePipelineConfig;
            PipelineConfig temporalPipelineConfig;

			Texture::Texture2D blueNoiseTexture;
            Buffer::Buffer volumetricUniformBuffer;

		};

	}

}

#endif