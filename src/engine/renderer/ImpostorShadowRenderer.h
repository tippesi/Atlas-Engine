#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class ImpostorShadowRenderer : public Renderer {

        public:
            ImpostorShadowRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(const Graphics::AttachmentFormats& attachmentFormats,
                Graphics::CommandList* commandList, RenderList* renderList, const RenderList::Pass* renderPass,
                mat4 lightViewMatrix, mat4 lightProjectionMatrix, vec3 lightLocation);

        private:
            Buffer::VertexArray vertexArray;

            PipelineConfig GetPipelineConfig(const Graphics::AttachmentFormats& attachmentFormats,
                bool interpolation, bool pixelDepthOffset, bool useInstanceBuffer);

        };

    }

}