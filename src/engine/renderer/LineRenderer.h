#pragma once

#include "Renderer.h"
#include "PrimitiveBatch.h"

#include "../texture/Texture2D.h"
#include "../graphics/UploadBufferPool.h"

namespace Atlas {

    namespace Renderer {

        // Draws the lines of a primitive batch over a texture which was rendered from the camera, as anti-aliased quads
        // with the width of the batch in pixels. Lines drawn into the scene would be blurred by temporal anti-aliasing and
        // thinned by upscalers, these stay sharp
        class LineRenderer : public Renderer {

        public:
            LineRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            /**
             * @param texture A texture of a format which can be rendered to
             * @param depthTexture The depth of the scene in the projection of the camera, at any resolution
             * @param occludedAlpha Batches which test the depth draw the parts behind the scene dashed with this factor,
             * 0 hides them. The others are drawn over everything
             */
            void Render(Texture::Texture2D* texture, Texture::Texture2D* depthTexture, const CameraComponent& camera,
                const PrimitiveBatch& batch, float occludedAlpha, Graphics::CommandList* commandList = nullptr);

        private:
            struct alignas(16) GPULine {
                vec4 from;
                vec4 to;
                uint32_t fromColor;
                uint32_t toColor;
            };

            struct alignas(16) PushConstants {
                mat4 pMatrix;
                vec2 invResolution;
                // To get the view depth of the scene back from its depth
                vec2 depthParameters;
                float width;
                float occludedAlpha;
                uint32_t testDepth;
            };

            PipelineConfig GetPipelineConfig(const Graphics::RenderingDesc& rendering);

            Graphics::UploadBufferPool uploadBufferPool;

        };

    }

}
