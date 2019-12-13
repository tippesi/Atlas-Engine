#ifndef AE_TEMPORALAARENDERER_H
#define AE_TEMPORALAARENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

		class TemporalAARenderer : public Renderer {

		public:
			TemporalAARenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Update(vec2 jitter);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			void GetUniforms();

			Shader::Shader shader;

			Buffer::VertexArray vertexArray;

			Shader::Uniform* convergence;
			Shader::Uniform* jitter;
			Shader::Uniform* invResolution;
			Shader::Uniform* resolution;

			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* ipvMatrixCurrent = nullptr;

			mat4 pvMatrixPrev;

		};

	}

}

#endif