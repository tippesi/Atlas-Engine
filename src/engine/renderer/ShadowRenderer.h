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

			static void AddConfig(OldShader::ShaderConfig* config);

			static void RemoveConfig(OldShader::ShaderConfig* config);

		private:
			void AdjustFaceCulling(bool cullFaces, bool& state);

			RenderList renderList;

			ImpostorShadowRenderer impostorRenderer;

			OldShader::Uniform* lightSpaceMatrixUniform = nullptr;
			OldShader::Uniform* modelMatrixUniform = nullptr;

			OldShader::Uniform* timeUniform = nullptr;

			OldShader::Uniform* vegetationUniform = nullptr;
			OldShader::Uniform* invertUVsUniform = nullptr;

			static OldShader::ShaderBatch shaderBatch;
			static std::mutex shaderBatchMutex;

		};

	}

}

#endif