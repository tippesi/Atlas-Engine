#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class ImpostorRenderer : public Renderer {

        public:
            ImpostorRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene,
                Graphics::CommandList* commandList, RenderList* renderList,
                std::unordered_map<void*, uint16_t>& materialMap, bool lateDraws = false);

            void Generate(const std::vector<mat4>& viewMatrices,
                mat4 projectionMatrix, float distToCenter, Mesh::Mesh* mesh, Mesh::Impostor* impostor);

        private:
            PipelineConfig GetPipelineConfig(const Graphics::AttachmentFormats& attachmentFormats,
                bool interpolation, bool pixelDepthOffset, bool useInstanceBuffer);

            PipelineConfig GetPipelineConfigForSubData(const Mesh::MeshSubData* subData,
                Mesh::Mesh* mesh, const Graphics::AttachmentFormats& attachmentFormats);

            Buffer::VertexArray vertexArray;

        };

    }

}