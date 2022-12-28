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

			Shader::Shader shader;

			Shader::Uniform* heightScale = nullptr;
			Shader::Uniform* tileScale = nullptr;
			Shader::Uniform* patchSize = nullptr;

			Shader::Uniform* nodeLocation = nullptr;
			Shader::Uniform* nodeSideLength = nullptr;

			Shader::Uniform* leftLoD = nullptr;
			Shader::Uniform* topLoD = nullptr;
			Shader::Uniform* rightLoD = nullptr;
			Shader::Uniform* bottomLoD = nullptr;

			Shader::Uniform* lightSpaceMatrix = nullptr;


		};

	}

}

#endif