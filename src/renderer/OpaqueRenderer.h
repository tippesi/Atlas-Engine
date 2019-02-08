#ifndef AE_GEOMETRYRENDERER_H
#define AE_GEOMETRYRENDERER_H

#include "../System.h"
#include "../shader/ShaderBatch.h"
#include "IRenderer.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		class OpaqueRenderer : public IRenderer {

		public:
			OpaqueRenderer();

			virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

			static void InitShaderBatch();

			static void AddConfig(Shader::ShaderConfig* config);

			static void RemoveConfig(Shader::ShaderConfig* config);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			Shader::Uniform* arrayMapUniform;
			Shader::Uniform* diffuseMapUniform;
			Shader::Uniform* normalMapUniform;
			Shader::Uniform* specularMapUniform;
			Shader::Uniform* heightMapUniform;

			Shader::Uniform* diffuseMapIndexUniform;
			Shader::Uniform* normalMapIndexUniform;
			Shader::Uniform* specularMapIndexUniform;
			Shader::Uniform* heightMapIndexUniform;

			Shader::Uniform* modelMatrixUniform;
			Shader::Uniform* viewMatrixUniform;
			Shader::Uniform* projectionMatrixUniform;

			Shader::Uniform* diffuseColorUniform;
			Shader::Uniform* specularColorUniform;
			Shader::Uniform* ambientColorUniform;
			Shader::Uniform* specularHardnessUniform;
			Shader::Uniform* specularIntensityUniform;

			static Shader::ShaderBatch shaderBatch;
			static std::mutex shaderBatchMutex;

		};


	}

}

#endif