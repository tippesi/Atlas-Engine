#ifndef AE_GEOMETRYRENDERER_H
#define AE_GEOMETRYRENDERER_H

#include "../System.h"
#include "../RenderList.h"

#include "Renderer.h"
#include "ImpostorRenderer.h"

#include <mutex>

namespace Atlas {

    namespace Renderer {

        class OpaqueRenderer : public Renderer {

        public:
            OpaqueRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
                Scene::Scene* scene, Graphics::CommandList* commandList, RenderList* renderList,
                std::unordered_map<void*, uint16_t> materialMap);

            void RenderImpostor(Viewport* viewport, const std::vector<mat4>& viewMatrices,
                mat4 projectionMatrix, Mesh::Mesh* mesh, Mesh::Impostor* impostor);

        private:
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