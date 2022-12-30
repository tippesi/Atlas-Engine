#ifndef AE_GEOMETRYRENDERER_H
#define AE_GEOMETRYRENDERER_H

#include "../System.h"
#include "../RenderList.h"
#include "../shader/ShaderBatch.h"

#include "Renderer.h"
#include "ImpostorRenderer.h"

#include <mutex>

namespace Atlas {

	namespace Renderer {

		class OpaqueRenderer : public Renderer {

		public:
			OpaqueRenderer() = default;

            void Init(GraphicsDevice* device);

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final {}

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
				Scene::Scene* scene, CommandList* commandList, RenderList* renderList,
                std::unordered_map<void*, uint16_t> materialMap);

			void RenderImpostor(Viewport* viewport, std::vector<mat4> viewMatrices,
				mat4 projectionMatrix, Mesh::Mesh* mesh, Mesh::Impostor* impostor);

		private:
			void AdjustFaceCulling(bool cullFaces, bool& state);

            GraphicsDevice* device;
			RenderList renderList;

			ImpostorRenderer impostorRenderer;

			struct PushConstants {
                uint32_t vegetation;
                uint32_t invertUVs;
                uint32_t twoSided;
                uint32_t staticMesh;
                uint32_t materialIdx;
                float normalScale;
                float displacementScale;
            };


		};


	}

}

#endif