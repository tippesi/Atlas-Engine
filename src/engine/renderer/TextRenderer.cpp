#include "TextRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void TextRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

            auto bufferUsage = Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::MultiBufferedBit
                               | Buffer::BufferUsageBits::HostAccessBit;
            instanceBuffer = Buffer::Buffer(bufferUsage, sizeof(vec4), 16384);
            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));

        }

        void TextRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

            return;

        }

        void TextRenderer::Render(Graphics::CommandList* commandList, Viewport* viewport, Font* font,
            const std::string& text, float x, float y, vec4 color, float scale,
            const Ref<Graphics::FrameBuffer>& frameBuffer) {

            float width = (float)(frameBuffer == nullptr ? viewport->width : frameBuffer->extent.width);
            float height = (float)(frameBuffer == nullptr ? viewport->height : frameBuffer->extent.height);

            vec4 clipArea = vec4(0.0f, 0.0f, width, height);
            vec4 blendArea = vec4(0.0f, 0.0f, width, height);

            RenderOutlined(commandList, viewport, font, text, x, y, color, vec4(1.0f), 0.0f,
                clipArea, blendArea, scale, frameBuffer);

        }

        void TextRenderer::Render(Graphics::CommandList* commandList, Viewport* viewport, Font* font,
            const std::string& text, float x, float y, vec4 color, vec4 clipArea, vec4 blendArea,
            float scale, const Ref<Graphics::FrameBuffer>& frameBuffer) {

            RenderOutlined(commandList, viewport, font, text, x, y, color, vec4(1.0f), 0.0f,
                clipArea, blendArea, scale, frameBuffer);

        }

        void TextRenderer::RenderOutlined(Graphics::CommandList* commandList, Viewport* viewport, Font* font,
            const std::string& text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
            float scale, const Ref<Graphics::FrameBuffer>& frameBuffer) {

            float width = (float)(frameBuffer == nullptr ? viewport->width : frameBuffer->extent.width);
            float height = (float)(frameBuffer == nullptr ? viewport->height : frameBuffer->extent.height);

            vec4 clipArea = vec4(0.0f, 0.0f, width, height);
            vec4 blendArea = vec4(0.0f, 0.0f, width, height);

            RenderOutlined(commandList, viewport, font, text, x, y, color, outlineColor, outlineScale,
                clipArea, blendArea, scale, frameBuffer);

        }

        void TextRenderer::RenderOutlined(Graphics::CommandList* commandList, Viewport* viewport, Font* font,
            const std::string& text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
            vec4 clipArea, vec4 blendArea, float scale, const Ref<Graphics::FrameBuffer>& frameBuffer) {

            int32_t characterCount;

            auto pipelineConfig = GeneratePipelineConfig(frameBuffer);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

            commandList->BindPipeline(pipeline);

            float width = float(frameBuffer == nullptr ? viewport->width : frameBuffer->extent.width);
            float height = float(frameBuffer == nullptr ? viewport->height : frameBuffer->extent.height);

            auto instances = CalculateCharacterInstances(font, text, &characterCount);
            instanceBuffer.SetData(instances.data(), frameCharacterCount, characterCount);

            Uniforms uniforms {
                .pMatrix = glm::ortho(0.0f, width, 0.0f, height),
                .clipArea = clipArea,
                .blendArea = blendArea,
                .characterColor = color,
                .outlineColor = outlineColor,
                .textOffset = vec2(x, y),
                .textScale = scale,
                .outlineScale = outlineScale,
                .edgeValue = float(font->edgeValue) / 255.0f,
                .smoothness = font->smoothness
            };

            uniformBuffer.SetData(&uniforms, 0, 1);

            commandList->BindImage(font->glyphTexture.image, font->glyphTexture.sampler, 3, 0);

            commandList->BindBuffer(font->glyphBuffer.Get(), 3, 1);

            commandList->BindBuffer(instanceBuffer.GetMultiBuffer(), 3, 2);
            commandList->BindBuffer(uniformBuffer.Get(), 3, 3);

            vertexArray.Bind(commandList);
            commandList->Draw(4, uint32_t(characterCount), 0, frameCharacterCount);

            frameCharacterCount += characterCount;

        }

        void TextRenderer::Update() {

            frameCharacterCount = 0;

        }

        std::vector<vec4> TextRenderer::CalculateCharacterInstances(Font* font, const std::string& text,
            int32_t* characterCount) {

            *characterCount = 0;

            auto instances = std::vector<vec4>(text.length());

            int32_t index = 0;

            float xOffset = 0.0f;

            auto ctext = text.c_str();

            auto nextGlyph = font->GetGlyphUTF8(ctext);

            while (nextGlyph->codepoint) {

                Glyph* glyph = nextGlyph;

                // Just visible characters should be rendered.
                if (glyph->codepoint > 32 && glyph->texArrayIndex < AE_GPU_GLYPH_COUNT) {
                    instances[index].x = glyph->offset.x + xOffset;
                    instances[index].y = glyph->offset.y + font->ascent;
                    instances[index].z = (float)glyph->texArrayIndex;
                    index++;
                }

                nextGlyph = font->GetGlyphUTF8(ctext);

                xOffset += glyph->advance + glyph->kern[nextGlyph->codepoint];

            }

            *characterCount = index;

            return instances;

        }

        PipelineConfig TextRenderer::GeneratePipelineConfig(const Ref<Graphics::FrameBuffer>& frameBuffer) {

            auto shaderConfig = ShaderConfig {
                {"text.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"text.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            Graphics::GraphicsPipelineDesc pipelineDesc {
                .swapChain = device->swapChain,
                .frameBuffer = frameBuffer,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
                .assemblyInputInfo = Graphics::Initializers::InitPipelineInputAssemblyStateCreateInfo(
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP),
                .depthStencilInputInfo = Graphics::Initializers::InitPipelineDepthStencilStateCreateInfo(
                    false, false, VK_COMPARE_OP_LESS_OR_EQUAL)
            };
            // By default, an alpha blending is used
            pipelineDesc.colorBlendAttachment.blendEnable = VK_TRUE;
            return PipelineConfig(shaderConfig, pipelineDesc);

        }

    }

}