#ifndef AE_SHADOWRENDERER_H
#define AE_SHADOWRENDERER_H

#include "../System.h"
#include "../RenderList.h"
#include "Renderer.h"
#include "ImpostorShadowRenderer.h"
#include "../shader/ShaderBatch.h"

#include <mutex>
#include <map>

namespace Atlas {

	namespace Renderer {

		class ShadowRenderer : public Renderer {

		public:
			ShadowRenderer() = default;

            void Init(GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, CommandList* commandList, RenderList* renderList);

		private:
            using LightMap = std::map<Lighting::Light*, Ref<FrameBuffer>>;

            struct PushConstants {
                mat4 lightSpaceMatrix;
                uint32_t vegetation;
                uint32_t invertUVs;
            };

            Ref<FrameBuffer> GetOrCreateFrameBuffer(Lighting::Light* light);

			void AdjustFaceCulling(bool cullFaces, bool& state);

            LightMap lightMap;

			ImpostorShadowRenderer impostorRenderer;

			OldShader::Uniform* lightSpaceMatrixUniform = nullptr;
			OldShader::Uniform* modelMatrixUniform = nullptr;

			OldShader::Uniform* timeUniform = nullptr;

			OldShader::Uniform* vegetationUniform = nullptr;
			OldShader::Uniform* invertUVsUniform = nullptr;

		};

	}

}

#endif