#ifndef AE_IMPOSTORRENDERER_H
#define AE_IMPOSTORRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class ImpostorRenderer : public Renderer {

		public:
			ImpostorRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {}

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
				RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap);

		private:
			void GetUniforms();

			void GetInterpolationUniforms();

			Shader::Shader shader;
			Shader::Shader interpolationShader;

			Buffer::VertexArray vertexArray;

			Shader::Uniform* vMatrixShader = nullptr;
			Shader::Uniform* pMatrixShader = nullptr;
			Shader::Uniform* cameraLocationShader = nullptr;

			Shader::Uniform* centerShader = nullptr;
			Shader::Uniform* radiusShader = nullptr;

			Shader::Uniform* viewsShader = nullptr;
			Shader::Uniform* cutoffShader = nullptr;
			Shader::Uniform* materialIdxShader = nullptr;

			Shader::Uniform* pvMatrixLastShader = nullptr;
			Shader::Uniform* jitterCurrentShader = nullptr;
			Shader::Uniform* jitterLastShader = nullptr;

			Shader::Uniform* vMatrixInterpolationShader = nullptr;
			Shader::Uniform* pMatrixInterpolationShader = nullptr;
			Shader::Uniform* cameraLocationInterpolationShader = nullptr;

			Shader::Uniform* centerInterpolationShader = nullptr;
			Shader::Uniform* radiusInterpolationShader = nullptr;

			Shader::Uniform* cameraRightInterpolationShader = nullptr;
			Shader::Uniform* cameraUpInterpolationShader = nullptr;

			Shader::Uniform* viewsInterpolationShader = nullptr;
			Shader::Uniform* cutoffInterpolationShader = nullptr;
			Shader::Uniform* materialIdxInterpolationShader = nullptr;

			Shader::Uniform* pvMatrixLastInterpolationShader = nullptr;
			Shader::Uniform* jitterCurrentInterpolationShader = nullptr;
			Shader::Uniform* jitterLastInterpolationShader = nullptr;

		};

	}

}

#endif