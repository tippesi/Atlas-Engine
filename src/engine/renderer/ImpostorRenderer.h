#ifndef AE_IMPOSTORRENDERER_H
#define AE_IMPOSTORRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class ImpostorRenderer : public Renderer {

		public:
			ImpostorRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
				RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap);

		private:
			void GetUniforms();

			void GetInterpolationUniforms();

            /*
			OldShader::ShaderBatch shaderBatch;

			OldShader::ShaderConfig normalConfig;
			OldShader::ShaderConfig interpolationConfig;
             */

			Buffer::VertexArray vertexArray;

            /*
			OldShader::Uniform* vMatrix = nullptr;
			OldShader::Uniform* pMatrix = nullptr;
			OldShader::Uniform* cameraLocation = nullptr;

			OldShader::Uniform* center = nullptr;
			OldShader::Uniform* radius = nullptr;

			OldShader::Uniform* cameraRight = nullptr;
			OldShader::Uniform* cameraUp = nullptr;

			OldShader::Uniform* views = nullptr;
			OldShader::Uniform* cutoff = nullptr;
			OldShader::Uniform* materialIdx = nullptr;

			OldShader::Uniform* pvMatrixLast = nullptr;
			OldShader::Uniform* jitterCurrent = nullptr;
			OldShader::Uniform* jitterLast = nullptr;
             */

		};

	}

}

#endif