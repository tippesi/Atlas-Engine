#ifndef AE_GEOMETRYRENDERER_H
#define AE_GEOMETRYRENDERER_H

#include "../System.h"
#include "../RenderList.h"
#include "../shader/ShaderBatch.h"

#include "Renderer.h"
#include "ImpostorRenderer.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		class OpaqueRenderer : public Renderer {

		public:
			OpaqueRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			virtual void RenderImpostor(Viewport* viewport, Framebuffer* framebuffer, std::vector<mat4> viewMatrices,
				mat4 projectionMatrix, Mesh::Mesh* mesh, Mesh::Impostor* impostor);

			static void InitShaderBatch();

			static void AddConfig(Shader::ShaderConfig* config);

			static void RemoveConfig(Shader::ShaderConfig* config);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			RenderList renderList;

			ImpostorRenderer impostorRenderer;

			Shader::Uniform* modelMatrixUniform = nullptr;
			Shader::Uniform* viewMatrixUniform = nullptr;
			Shader::Uniform* projectionMatrixUniform = nullptr;

			Shader::Uniform* cameraLocationUniform = nullptr;
			Shader::Uniform* diffuseColorUniform = nullptr;
			Shader::Uniform* emissiveColorUniform = nullptr;
			Shader::Uniform* specularColorUniform = nullptr;
			Shader::Uniform* ambientColorUniform = nullptr;
			Shader::Uniform* specularHardnessUniform = nullptr;
			Shader::Uniform* specularIntensityUniform = nullptr;
			Shader::Uniform* normalScaleUniform = nullptr;
			Shader::Uniform* displacementScaleUniform = nullptr;

			Shader::Uniform* timeUniform = nullptr;
			Shader::Uniform* deltaTimeUniform = nullptr;

			Shader::Uniform* vegetationUniform = nullptr;
			Shader::Uniform* invertUVsUniform = nullptr;

			Shader::Uniform* pvMatrixLast = nullptr;

			Shader::Uniform* jitterCurrent = nullptr;
			Shader::Uniform* jitterLast = nullptr;

			static Shader::ShaderBatch shaderBatch;
			static std::mutex shaderBatchMutex;

		};


	}

}

#endif