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

			OldShader::OldShader shader;

			Buffer::VertexArray vertexArray;

			mat4 pvMatrixPrev = mat4(1.0f);

		};

	}

}

#endif