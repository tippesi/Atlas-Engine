#ifndef AE_TERRAINSHADOWRENDERER_H
#define AE_TERRAINSHADOWRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class TerrainShadowRenderer : public Renderer {

		public:
			TerrainShadowRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

		private:
			void GetUniforms();

			OldShader::OldShader shader;

			OldShader::Uniform* heightScale = nullptr;
			OldShader::Uniform* tileScale = nullptr;
			OldShader::Uniform* patchSize = nullptr;

			OldShader::Uniform* nodeLocation = nullptr;
			OldShader::Uniform* nodeSideLength = nullptr;

			OldShader::Uniform* leftLoD = nullptr;
			OldShader::Uniform* topLoD = nullptr;
			OldShader::Uniform* rightLoD = nullptr;
			OldShader::Uniform* bottomLoD = nullptr;

			OldShader::Uniform* lightSpaceMatrix = nullptr;


		};

	}

}

#endif