#ifndef AE_TEXTRENDERER_H
#define AE_TEXTRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../Font.h"
#include "buffer/VertexArray.h"

namespace Atlas {

    namespace Renderer {

        class TextRenderer : public Renderer {

        public:
            TextRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

            void Render(Graphics::CommandList* commandList, Viewport* viewport, Font* font, const std::string& text,
                float x, float y, vec4 color = vec4(1.0f), float scale = 1.0f,
                const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void Render(Graphics::CommandList* commandList, Viewport* viewport, Font* font, const std::string& text,
                float x, float y, vec4 color, vec4 clipArea, vec4 blendArea, float scale = 1.0f,
                const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void RenderOutlined(Graphics::CommandList* commandList, Viewport* viewport, Font* font,
                const std::string& text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
                float scale = 1.0f, const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void RenderOutlined(Graphics::CommandList* commandList, Viewport* viewport, Font* font,
                const std::string& text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
                vec4 clipArea, vec4 blendArea, float scale = 1.0f, const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void Update();

        private:
            struct alignas(16) Uniforms {
                mat4 pMatrix;
                vec4 clipArea;
                vec4 blendArea;
                vec4 characterColor;
                vec4 outlineColor;
                vec2 textOffset;
                float textScale;
                float outlineScale;
                float edgeValue;
                float smoothness;
            };

            std::vector<vec4> CalculateCharacterInstances(Font* font, const std::string& text, int32_t* characterCount);

            PipelineConfig GeneratePipelineConfig(const Ref<Graphics::FrameBuffer>& frameBuffer);

            Buffer::VertexArray vertexArray;

            Buffer::Buffer instanceBuffer;
            Buffer::UniformBuffer uniformBuffer;

            Ref<Graphics::RenderPass> renderPass;

            uint32_t frameCharacterCount = 0;

        };


    }

}

#endif