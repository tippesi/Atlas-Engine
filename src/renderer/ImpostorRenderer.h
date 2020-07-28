#ifndef AE_IMPOSTORRENDERER_H
#define AE_IMPOSTORRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../shader/ShaderBatch.h"

namespace Atlas {

	namespace Renderer {

		class ImpostorRenderer : public Renderer {

		public:
			ImpostorRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
				RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap);

		private:
			void GetUniforms();

			void GetInterpolationUniforms();

			Shader::ShaderBatch shaderBatch;

			Shader::ShaderConfig normalConfig;
			Shader::ShaderConfig interpolationConfig;

			Buffer::VertexArray vertexArray;

			Shader::Uniform* vMatrix = nullptr;
			Shader::Uniform* pMatrix = nullptr;
			Shader::Uniform* cameraLocation = nullptr;

			Shader::Uniform* center = nullptr;
			Shader::Uniform* radius = nullptr;

			Shader::Uniform* cameraRight = nullptr;
			Shader::Uniform* cameraUp = nullptr;

			Shader::Uniform* views = nullptr;
			Shader::Uniform* cutoff = nullptr;
			Shader::Uniform* materialIdx = nullptr;

			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* jitterCurrent = nullptr;
			Shader::Uniform* jitterLast = nullptr;

		};

	}

}

#endif