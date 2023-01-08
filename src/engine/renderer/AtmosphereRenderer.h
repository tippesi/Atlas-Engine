#ifndef AE_ATMOSPHERERENDERER_H
#define AE_ATMOSPHERERENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../lighting/EnvironmentProbe.h"

namespace Atlas {

	namespace Renderer {


		class AtmosphereRenderer : public Renderer {

		public:
			AtmosphereRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

			void Render(Lighting::EnvironmentProbe* probe, Scene::Scene* scene);

			static std::string vertexPath;
			static std::string fragmentPath;

		private:
			Buffer::VertexArray vertexArray;

			//OldShader::OldShader shader;

		};

	}

}

#endif
