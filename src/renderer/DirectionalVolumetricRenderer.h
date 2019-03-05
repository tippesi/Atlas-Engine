#ifndef AE_DIRECTIONALVOLUMETRICRENDERER_H
#define AE_DIRECTIONALVOLUMETRICRENDERER_H

#include "../System.h"
#include "../Kernel.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class DirectionalVolumetricRenderer : public Renderer{

		public:
			DirectionalVolumetricRenderer();

			virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static std::string volumetricVertexPath;
			static std::string volumetricFragmentPath;
			static std::string bilateralBlurVertexPath;
			static std::string bilateralBlurFragmentPath;

		private:
			void GetVolumetricUniforms();
			void GetBilateralBlurUniforms();

			Framebuffer* framebuffer;

			Kernel blurKernel;

			Shader::Shader volumetricShader;
			Shader::Shader bilateralBlurShader;

			// Volumetric shader uniforms
			Shader::Uniform* depthTexture;
			Shader::Uniform* shadowTexture;
			Shader::Uniform* lightDirection;
			Shader::Uniform* inverseProjectionMatrix;
			Shader::Uniform* sampleCount;
			Shader::Uniform* scattering;
			Shader::Uniform* framebufferResolution;

			Shader::Uniform* shadowCascadeCount;

			struct ShadowCascadeUniform {
				Shader::Uniform* distance;
				Shader::Uniform* lightSpace;
			}cascades[MAX_SHADOW_CASCADE_COUNT];

			// Bilateral blur shader uniforms
			Shader::Uniform* diffuseTexture;
			Shader::Uniform* bilateralDepthTexture;
			Shader::Uniform* blurDirection;
			Shader::Uniform* offsets;
			Shader::Uniform* weights;
			Shader::Uniform* kernelSize;

		};

	}

}

#endif