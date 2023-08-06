#ifndef AE_IMPOSTORRENDERER_H
#define AE_IMPOSTORRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class ImpostorRenderer : public Renderer {

        public:
            ImpostorRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene,
                Graphics::CommandList* commandList, RenderList* renderList,
                std::unordered_map<void*, uint16_t> materialMap);

            void Generate(Viewport* viewport, const std::vector<mat4>& viewMatrices,
                mat4 projectionMatrix, float distToCenter, Mesh::Mesh* mesh, Mesh::Impostor* impostor);

        private:
            Ref<Graphics::FrameBuffer> GenerateFrameBuffer(Mesh::Impostor* impostor);

            PipelineConfig GetPipelineConfig(Ref<Graphics::FrameBuffer>& frameBuffer,
                bool interpolation, bool pixelDepthOffset);

            PipelineConfig GetPipelineConfigForSubData(Mesh::MeshSubData* subData,
                Mesh::Mesh* mesh, Ref<Graphics::FrameBuffer>& frameBuffer);

            Buffer::VertexArray vertexArray;

        };

    }

}

#endif