#pragma once

#include "Renderer.h"

#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "texture/Texture3D.h"

namespace Atlas {

    namespace Renderer {

        class TextureRenderer : public Renderer {

        public:
            TextureRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void RenderTexture2D(Graphics::CommandList* commandList, Ref<Viewport> viewport, Texture::Texture2D* texture,
                float x, float y, float width, float height, float rotation = 0.0f, float brightness = 1.0f,
                bool alphaBlending = false, bool invert = false);

            void RenderTexture2D(Graphics::CommandList* commandList, Ref<Viewport> viewport, Texture::Texture2D* texture,
                float x, float y, float width, float height, vec4 clipArea, vec4 blendArea,
                float rotation = 0.0f, float brightness = 1.0f, bool alphaBlending = false, bool invert = false);

            void RenderTexture2DArray(Graphics::CommandList* commandList, Ref<Viewport> viewport, Texture::Texture2DArray* texture,
                int32_t depth, float x, float y, float width, float height, bool alphaBlending = false, bool invert = false);

            void RenderTexture2DArray(Graphics::CommandList* commandList, Ref<Viewport> viewport, Texture::Texture2DArray* texture,
                int32_t depth, float x, float y, float width, float height, vec4 clipArea, vec4 blendArea,
                bool alphaBlending = false, bool invert = false);

            void RenderTexture3D(Graphics::CommandList* commandList, Ref<Viewport> viewport, Texture::Texture3D* texture,
                float depth, float x, float y, float width, float height, bool alphaBlending = false, bool invert = false);

            void RenderTexture3D(Graphics::CommandList* commandList, Ref<Viewport> viewport, Texture::Texture3D* texture,
                float depth, float x, float y, float width, float height,
                vec4 clipArea, vec4 blendArea, bool alphaBlending = false, bool invert = false);

        private:
            struct alignas(16) PushConstants {
                mat4 pMatrix;
                vec4 blendArea;
                vec4 clipArea;
                vec2 offset;
                vec2 scale;
                int invert;
                float depth;
                float rotation;
                float brightness;
            };

            void Draw(Graphics::CommandList* commandList, Ref<Viewport> viewport, Texture::Texture* texture,
                float depth, float x, float y, float width, float height, vec4 clipArea, vec4 blendArea,
                float rotation, float brightness, bool alphaBlending, bool invert, const std::string& macro);

            PipelineConfig GeneratePipelineConfig(const Ref<Graphics::FrameBuffer>& frameBuffer,
                const std::vector<std::string>& macros);

            Buffer::VertexArray vertexArray;

        };

    }

}