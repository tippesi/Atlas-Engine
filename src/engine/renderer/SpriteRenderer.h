#pragma once

#include "Renderer.h"

#include "../texture/Texture2D.h"
#include "../graphics/UploadBufferPool.h"

#include <span>

namespace Atlas {

    namespace Renderer {

        // A camera facing quad with a texture or a part of an atlas, e.g. an icon
        struct Sprite {
            vec3 position = vec3(0.0f);
            // In world units, or in pixels of the target when the size is in screen space
            vec2 size = vec2(1.0f);
            bool screenSpaceSize = false;

            vec4 color = vec4(1.0f);

            // Drawn around the opaque parts of the texture, keeps e.g. icons readable on any background
            vec4 outlineColor = vec4(0.0f);
            // In pixels of the target
            float outlineWidth = 0.0f;

            // The part of the texture, e.g. a cell of an atlas
            vec2 uvMin = vec2(0.0f);
            vec2 uvMax = vec2(1.0f);

            Texture::Texture2D* texture = nullptr;
        };

        class SpriteRenderer : public Renderer {

        public:
            SpriteRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            /**
             * Draws sprites over a texture which was rendered from the camera
             * @param texture A texture of a format which can be rendered to
             * @param depthTexture The depth of the scene in the projection of the camera, at any resolution
             * @param occludedAlpha The scene depth hides sprites behind it, or fades them with this factor
             * @note Sprites are drawn in their order, consecutive ones with the same texture in one draw
             */
            void Render(Texture::Texture2D* texture, Texture::Texture2D* depthTexture, const CameraComponent& camera,
                std::span<const Sprite> sprites, float occludedAlpha = 0.0f, Graphics::CommandList* commandList = nullptr);

        private:
            struct alignas(16) GPUSprite {
                // The w component is 1 for a size in screen space
                vec4 viewPosition;
                vec4 uvMinMax;
                vec2 size;
                uint32_t color;
                uint32_t outlineColor;
                float outlineWidth;
            };

            struct alignas(16) PushConstants {
                mat4 pMatrix;
                vec2 invResolution;
                float occludedAlpha;
            };

            PipelineConfig GetPipelineConfig(const Graphics::RenderingDesc& rendering);

            Graphics::UploadBufferPool uploadBufferPool;

        };

    }

}
