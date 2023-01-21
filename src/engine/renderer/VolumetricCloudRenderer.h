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
			void GenerateShapeTexture(Graphics::CommandList* commandList,
                Texture::Texture3D* texture, float baseScale);
			
			void GenerateDetailTexture(Graphics::CommandList* commandList,
                Texture::Texture3D* texture, float baseScale);

            PipelineConfig shapeNoisePipelineConfig;
            PipelineConfig detailNoisePipelineConfig;

            PipelineConfig integratePipelineConfig;
            PipelineConfig temporalPipelineConfig;

			Texture::Texture2D blueNoiseTexture;

		};

	}

}

#endif