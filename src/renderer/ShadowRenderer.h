#ifndef AE_SHADOWRENDERER_H
#define AE_SHADOWRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "../shader/ShaderBatch.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		class ShadowRenderer : public IRenderer {

		public:
			ShadowRenderer();

			virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

			static void InitShaderBatch();

			static void AddConfig(Shader::ShaderConfig* config);

			static void RemoveConfig(Shader::ShaderConfig* config);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			Framebuffer* framebuffer;

			Shader::Uniform* arrayMapUniform;
			Shader::Uniform* diffuseMapUniform;
			Shader::Uniform* lightSpaceMatrixUniform;
			Shader::Uniform* modelMatrixUniform;

			static Shader::ShaderBatch shaderBatch;
			static std::mutex shaderBatchMutex;

		};

	}

}

#endif