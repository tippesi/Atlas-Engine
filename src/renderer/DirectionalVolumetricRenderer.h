#ifndef AE_DIRECTIONALVOLUMETRICRENDERER_H
#define AE_DIRECTIONALVOLUMETRICRENDERER_H

#include "../System.h"
#include "../Filter.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class DirectionalVolumetricRenderer : public Renderer{

		public:
			DirectionalVolumetricRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

			static std::string volumetricVertexPath;
			static std::string volumetricFragmentPath;
			static std::string bilateralBlurVertexPath;
			static std::string bilateralBlurFragmentPath;

		private:
			void GetVolumetricUniforms();
			void GetBilateralBlurUniforms();

			Framebuffer framebuffer;

			Filter blurFilter;

			Shader::Shader volumetricShader;
			Shader::Shader bilateralBlurShader;

			// Volumetric shader uniforms
			Shader::Uniform* lightDirection = nullptr;
			Shader::Uniform* inverseProjectionMatrix = nullptr;
			Shader::Uniform* sampleCount = nullptr;
			Shader::Uniform* framebufferResolution = nullptr;

			Shader::Uniform* shadowCascadeCount = nullptr;

			struct ShadowCascadeUniform {
				Shader::Uniform* distance = nullptr;
				Shader::Uniform* lightSpace = nullptr;
			}cascades[MAX_SHADOW_CASCADE_COUNT + 1];

			// Bilateral blur shader uniforms
			Shader::Uniform* blurDirection = nullptr;
			Shader::Uniform* offsets = nullptr;
			Shader::Uniform* weights = nullptr;
			Shader::Uniform* kernelSize = nullptr;

		};

	}

}

#endif