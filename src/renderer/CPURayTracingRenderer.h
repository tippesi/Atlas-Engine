#ifndef AE_CPURAYTRACINGRENDERER_H
#define AE_CPURAYTRACINGRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class CPURayTracingRenderer : public Renderer {

		public:
			CPURayTracingRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Viewport* viewport, Texture::Texture2D* texture, Camera* camera, Scene::Scene* scene);

			static std::string unprojectionComputePath;

		private:
			Shader::Shader unprojectionShader;

			Texture::Texture2D rayOriginsTexture;
			Texture::Texture2D rayDirectionTexture;

			Shader::Uniform* inverseViewMatrix;
			Shader::Uniform* inverseProjectionMatrix;



		};

	}

}

#endif