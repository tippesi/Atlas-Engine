#ifndef AE_GEOMETRYRENDERER_H
#define AE_GEOMETRYRENDERER_H

#include "../System.h"
#include "../RenderList.h"
#include "../shader/ShaderBatch.h"
#include "Renderer.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		class OpaqueRenderer : public Renderer {

		public:
			OpaqueRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			static void InitShaderBatch();

			static void AddConfig(Shader::ShaderConfig* config);

			static void RemoveConfig(Shader::ShaderConfig* config);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			RenderList renderList;

			Shader::Uniform* modelMatrixUniform = nullptr;
			Shader::Uniform* viewMatrixUniform = nullptr;
			Shader::Uniform* projectionMatrixUniform = nullptr;

			Shader::Uniform* diffuseColorUniform = nullptr;
			Shader::Uniform* specularColorUniform = nullptr;
			Shader::Uniform* ambientColorUniform = nullptr;
			Shader::Uniform* specularHardnessUniform = nullptr;
			Shader::Uniform* specularIntensityUniform = nullptr;

			static Shader::ShaderBatch shaderBatch;
			static std::mutex shaderBatchMutex;

		};


	}

}

#endif