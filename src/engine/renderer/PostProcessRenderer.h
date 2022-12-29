#ifndef AE_POSTPROCESSRENDERER_H
#define AE_POSTPROCESSRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "shader/OldShader.h"

namespace Atlas {

	namespace Renderer {

		class PostProcessRenderer : public Renderer {

		public:
			PostProcessRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

		private:
			void GetUniforms();

			OldShader::OldShader shader;
			OldShader::OldShader sharpenShader;

			OldShader::Uniform* hdrTextureResolution = nullptr;
			OldShader::Uniform* exposure = nullptr;
			OldShader::Uniform* saturation = nullptr;
			OldShader::Uniform* bloomPassses = nullptr;
			OldShader::Uniform* aberrationStrength = nullptr;
			OldShader::Uniform* aberrationReversed = nullptr;
			OldShader::Uniform* vignetteOffset = nullptr;
			OldShader::Uniform* vignettePower = nullptr;
			OldShader::Uniform* vignetteStrength = nullptr;
			OldShader::Uniform* vignetteColor = nullptr;
			OldShader::Uniform* sharpenFactor = nullptr;
			OldShader::Uniform* timeInMilliseconds = nullptr;

		};

	}

}

#endif