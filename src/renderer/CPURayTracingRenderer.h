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
			vec3 EvaluateLight(Lighting::DirectionalLight* light, int32_t index, vec2 barrycentric);

			void UpdateData(Scene::Scene* scene);

			struct Triangle {
				vec3 v0;
				vec3 v1;
				vec3 v2;
				vec3 n0;
				vec3 n1;
				vec3 n2;
				int32_t materialIndex;
			};

			std::vector<Triangle> triangles;
			std::vector<Material*> materials;

		};

	}

}

#endif