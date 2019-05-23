#ifndef AE_POSTPROCESSRENDERER_H
#define AE_POSTPROCESSRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "../shader/Shader.h"

namespace Atlas {

	namespace Renderer {

		class PostProcessRenderer : public Renderer {

		public:
			PostProcessRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Shader::Shader shader;

			Shader::Uniform* hdrTextureResolution = nullptr;
			Shader::Uniform* exposure = nullptr;
			Shader::Uniform* saturation = nullptr;
			Shader::Uniform* bloomPassses = nullptr;
			Shader::Uniform* aberrationStrength = nullptr;
			Shader::Uniform* aberrationReversed = nullptr;
			Shader::Uniform* vignetteOffset = nullptr;
			Shader::Uniform* vignettePower = nullptr;
			Shader::Uniform* vignetteStrength = nullptr;
			Shader::Uniform* vignetteColor = nullptr;
			Shader::Uniform* timeInMilliseconds = nullptr;

		};

	}

}

#endif