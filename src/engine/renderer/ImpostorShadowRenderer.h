#ifndef AE_IMPOSTORSHADOWRENDERER_H
#define AE_IMPOSTORSHADOWRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class ImpostorShadowRenderer : public Renderer {

        public:
            ImpostorShadowRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<Graphics::FrameBuffer>& frameBuffer, RenderList* renderList,
                Graphics::CommandList* commandList, RenderList::Pass* renderPass,
                mat4 lightSpaceMatrix, vec3 lightLocation);

        private:
            Buffer::VertexArray vertexArray;

            PipelineConfig GetPipelineConfig(Ref<Graphics::FrameBuffer>& frameBuffer);

        };

    }

}

#endif