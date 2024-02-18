#pragma once

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

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

            void Render(Graphics::CommandList* commandList, Ref<Viewport> viewport, Ref<Font> font, const std::string& text,
                float x, float y, vec4 color = vec4(1.0f), float scale = 1.0f,
                const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void Render(Graphics::CommandList* commandList, Ref<Viewport> viewport, Ref<Font> font, const std::string& text,
                float x, float y, vec4 color, vec4 clipArea, vec4 blendArea, float scale = 1.0f,
                const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void RenderOutlined(Graphics::CommandList* commandList, Ref<Viewport> viewport, Ref<Font> font,
                const std::string& text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
                float scale = 1.0f, const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void RenderOutlined(Graphics::CommandList* commandList, Ref<Viewport> viewport, Ref<Font> font,
                const std::string& text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
                vec4 clipArea, vec4 blendArea, float scale = 1.0f, const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void RenderOutlined3D(Graphics::CommandList* commandList, Ref<Font> font, const std::string& text, 
                vec3 position, quat rotation, vec2 halfSize, vec4 color, vec4 outlineColor, float outlineScale,
                float scale = 1.0f, const Ref<Graphics::FrameBuffer>& frameBuffer = nullptr);

            void Update();

        private:
            struct alignas(16) PushConstants {
                vec4 clipArea;
                vec4 blendArea;
                vec4 characterColor;
                vec4 outlineColor;
                vec2 textOffset;
                vec2 renderArea;
                float textScale;
                float outlineScale;
                float edgeValue;
                float smoothness;
            };

            struct alignas(16) PushConstants3D {
                vec4 position;
                vec4 right;
                vec4 down;
                vec4 characterColor;
                vec4 outlineColor;
                vec2 renderHalfSize;
                float textScale;
                float outlineScale;
                float edgeValue;
                float smoothness;
            };

            std::vector<vec4> CalculateCharacterInstances(Ref<Font>& font, const std::string& text, int32_t* characterCount);

            std::vector<vec4> CalculateCharacterInstances3D(Ref<Font>& font, const std::string& text, vec2 halfSize,
                float textScale, int32_t* characterCount);

            PipelineConfig GetPipelineConfig(const Ref<Graphics::FrameBuffer>& frameBuffer, bool threeDimensional = false);

            Buffer::Buffer instanceBuffer;

            Ref<Graphics::RenderPass> renderPass;

            uint32_t frameCharacterCount = 0;

        };


    }

}