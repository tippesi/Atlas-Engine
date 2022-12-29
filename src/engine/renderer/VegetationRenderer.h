#ifndef AE_VEGETATIONRENDERER_H
#define AE_VEGETATIONRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "helper/VegetationHelper.h"

namespace Atlas {

	namespace Renderer {

		class VegetationRenderer : public Renderer {

		public:
			VegetationRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
				Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap);

			Helper::VegetationHelper helper;

		private:
			void DepthPrepass(Scene::Vegetation& vegetation, std::vector<Mesh::VegetationMesh*>& meshes, 
				Camera* camera, float time, float deltaTime);

			OldShader::OldShader shader;
			OldShader::OldShader depthShader;

		};

	}

}

#endif