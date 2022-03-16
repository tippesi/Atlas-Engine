#ifndef AE_SHADOWRENDERER_H
#define AE_SHADOWRENDERER_H

#include "../System.h"
#include "../RenderList.h"
#include "Renderer.h"
#include "ImpostorShadowRenderer.h"
#include "../shader/ShaderBatch.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		class ShadowRenderer : public Renderer {

		public:
			ShadowRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

			static void InitShaderBatch();

			static void AddConfig(Shader::ShaderConfig* config);

			static void RemoveConfig(Shader::ShaderConfig* config);

		private:
			void AdjustFaceCulling(bool cullFaces, bool& state);

			RenderList renderList;

			ImpostorShadowRenderer impostorRenderer;

			Framebuffer framebuffer;

			Shader::Uniform* lightSpaceMatrixUniform = nullptr;
			Shader::Uniform* modelMatrixUniform = nullptr;

			Shader::Uniform* timeUniform = nullptr;

			Shader::Uniform* vegetationUniform = nullptr;
			Shader::Uniform* invertUVsUniform = nullptr;

			static Shader::ShaderBatch shaderBatch;
			static std::mutex shaderBatchMutex;

		};

	}

}

#endif