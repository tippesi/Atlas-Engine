#ifndef AE_CPURAYTRACINGRENDERER_H
#define AE_CPURAYTRACINGRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		class CPURayTracingRenderer : public Renderer {

		public:
			CPURayTracingRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Viewport* viewport, Texture::Texture2D* texture, Camera* camera, Scene::Scene* scene);

			static std::string unprojectionComputePath;

		private:
			void PreprocessActors(std::vector<Actor::MeshActor*> &actors);

			std::unordered_map<Actor::MeshActor*, std::vector<vec3>> transformedVertices;

		};

	}

}

#endif