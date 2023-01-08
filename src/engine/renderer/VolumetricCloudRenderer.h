#ifndef AE_VOLUMETRICCLOUDRENDERER_H
#define AE_VOLUMETRICCLOUDRENDERER_H

#include "Renderer.h"

#include "texture/Texture3D.h"

namespace Atlas {

	namespace Renderer {

		class VolumetricCloudRenderer : public Renderer {

		public:
			VolumetricCloudRenderer();

			void Render(Viewport* viewport, RenderTarget* target,
				Camera* camera, Scene::Scene* scene) final;

			void GenerateTextures(Scene::Scene* scene);

		private:
			void GenerateShapeTexture(Texture::Texture3D* texture, float baseScale);
			
			void GenerateDetailTexture(Texture::Texture3D* texture, float baseScale);

            /*
			OldShader::OldShader shapeNoiseShader;
			OldShader::OldShader detailNoiseShader;
			OldShader::OldShader integrateShader;
			OldShader::OldShader temporalShader;
            */
			
			Texture::Texture2D blueNoiseTexture;

		};

	}

}

#endif