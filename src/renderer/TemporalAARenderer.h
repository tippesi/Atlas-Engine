#ifndef AE_TEMPORALAARENDERER_H
#define AE_TEMPORALAARENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class TemporalAARenderer : public Renderer {

		public:
			TemporalAARenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

		private:
			void GetUniforms();

			Shader::Shader shader;

			Buffer::VertexArray vertexArray;

			Shader::Uniform* convergence = nullptr;
			Shader::Uniform* invResolution = nullptr;
			Shader::Uniform* resolution = nullptr;

			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* ipvMatrixCurrent = nullptr;

			mat4 pvMatrixPrev;

		};

	}

}

#endif