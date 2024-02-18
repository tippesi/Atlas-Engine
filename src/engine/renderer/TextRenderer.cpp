#include "TextRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void TextRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            auto bufferUsage = Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::MultiBufferedBit
                               | Buffer::BufferUsageBits::HostAccessBit;
            instanceBuffer = Buffer::Buffer(bufferUsage, sizeof(vec4), 16384);

        }

        void TextRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto textSubset = scene->GetSubset<TextComponent>();

            // This whole thing isn't really optimized right now, but as long as it's not a bottleneck it's fine right now
            for (auto entity : textSubset) {

                auto textComp = textSubset.Get(entity);
                if (!textComp.font.IsLoaded())
                    continue;
                
                RenderOutlined3D(commandList, textComp.font.Get(), textComp.text, textComp.GetTransformedPosition(),
                    textComp.GetTransformedRotation(), textComp.halfSize, textComp.textColor, textComp.outlineColor,
                    textComp.outlineFactor, textComp.textScale, target->afterLightingFrameBuffer);

            }

        }

        void TextRenderer::Render(Graphics::CommandList* commandList, Ref<Viewport> viewport, Ref<Font> font,
            const std::string& text, float x, float y, vec4 color, float scale,
            const Ref<Graphics::FrameBuffer>& frameBuffer) {

            float width = (float)(frameBuffer == nullptr ? viewport->width : frameBuffer->extent.width);
            float height = (float)(frameBuffer == nullptr ? viewport->height : frameBuffer->extent.height);

            vec4 clipArea = vec4(0.0f, 0.0f, width, height);
            vec4 blendArea = vec4(0.0f, 0.0f, width, height);

            RenderOutlined(commandList, viewport, font, text, x, y, color, vec4(1.0f), 0.0f,
                clipArea, blendArea, scale, frameBuffer);

        }

        void TextRenderer::Render(Graphics::CommandList* commandList, Ref<Viewport> viewport, Ref<Font> font,
            const std::string& text, float x, float y, vec4 color, vec4 clipArea, vec4 blendArea,
            float scale, const Ref<Graphics::FrameBuffer>& frameBuffer) {

            RenderOutlined(commandList, viewport, font, text, x, y, color, vec4(1.0f), 0.0f,
                clipArea, blendArea, scale, frameBuffer);

        }

        void TextRenderer::RenderOutlined(Graphics::CommandList* commandList, Ref<Viewport> viewport, Ref<Font> font,
            const std::string& text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
            float scale, const Ref<Graphics::FrameBuffer>& frameBuffer) {

            float width = (float)(frameBuffer == nullptr ? viewport->width : frameBuffer->extent.width);
            float height = (float)(frameBuffer == nullptr ? viewport->height : frameBuffer->extent.height);

            vec4 clipArea = vec4(0.0f, 0.0f, width, height);
            vec4 blendArea = vec4(0.0f, 0.0f, width, height);

            RenderOutlined(commandList, viewport, font, text, x, y, color, outlineColor, outlineScale,
                clipArea, blendArea, scale, frameBuffer);

        }

        void TextRenderer::RenderOutlined(Graphics::CommandList* commandList, Ref<Viewport> viewport, Ref<Font> font,
            const std::string& text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
            vec4 clipArea, vec4 blendArea, float scale, const Ref<Graphics::FrameBuffer>& frameBuffer) {

            int32_t characterCount;

            auto pipelineConfig = GetPipelineConfig(frameBuffer);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

            commandList->BindPipeline(pipeline);

            float width = float(frameBuffer == nullptr ? viewport->width : frameBuffer->extent.width);
            float height = float(frameBuffer == nullptr ? viewport->height : frameBuffer->extent.height);

            auto instances = CalculateCharacterInstances(font, text, &characterCount);
            instanceBuffer.SetData(instances.data(), frameCharacterCount, characterCount);

            PushConstants constants {
                .clipArea = clipArea,
                .blendArea = blendArea,
                .characterColor = color,
                .outlineColor = outlineColor,
                .textOffset = vec2(x, y),
                .renderArea = vec2(width, height),
                .textScale = scale,
                .outlineScale = outlineScale,
                .edgeValue = float(font->edgeValue) / 255.0f,
                .smoothness = font->smoothness
            };

            commandList->PushConstants("constants", &constants);

            commandList->BindImage(font->glyphTexture.image, font->glyphTexture.sampler, 3, 0);

            commandList->BindBuffer(font->glyphBuffer.Get(), 3, 1);
            commandList->BindBuffer(instanceBuffer.GetMultiBuffer(), 3, 2);

            commandList->Draw(4, uint32_t(characterCount), 0, frameCharacterCount);

            frameCharacterCount += characterCount;

        }

        void TextRenderer::RenderOutlined3D(Graphics::CommandList* commandList, Ref<Font> font, const std::string& text, 
                vec3 position, quat rotation, vec2 halfSize, vec4 color, vec4 outlineColor, float outlineScale,
                float scale, const Ref<Graphics::FrameBuffer>& frameBuffer) {

            scale /= 16.0f;

            int32_t characterCount;

            auto pipelineConfig = GetPipelineConfig(frameBuffer, true);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

            commandList->BindPipeline(pipeline);

            auto instances = CalculateCharacterInstances3D(font, text, halfSize, scale, &characterCount);
            instanceBuffer.SetData(instances.data(), frameCharacterCount, characterCount);

            auto right = glm::mat3_cast(rotation) * glm::vec3(1.0f, 0.0f, 0.0f);
            auto down = glm::mat3_cast(rotation) * glm::vec3(0.0f, -1.0f, 0.0f);

            PushConstants3D constants {
                .position = vec4(position, 1.0f),
                .right = vec4(right, 1.0f),
                .down = vec4(down, 1.0f),
                .characterColor = color,
                .outlineColor = outlineColor,
                .renderHalfSize = halfSize,
                .textScale = scale,
                .outlineScale = outlineScale,
                .edgeValue = float(font->edgeValue) / 255.0f,
                .smoothness = font->smoothness
            };

            commandList->PushConstants("constants", &constants);

            commandList->BindImage(font->glyphTexture.image, font->glyphTexture.sampler, 3, 0);

            commandList->BindBuffer(font->glyphBuffer.Get(), 3, 1);
            commandList->BindBuffer(instanceBuffer.GetMultiBuffer(), 3, 2);

            commandList->Draw(4, uint32_t(characterCount), 0, frameCharacterCount);

            frameCharacterCount += characterCount;

        }

        void TextRenderer::Update() {

            frameCharacterCount = 0;

        }

        std::vector<vec4> TextRenderer::CalculateCharacterInstances(Ref<Font>& font, const std::string& text,
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

        std::vector<vec4> TextRenderer::CalculateCharacterInstances3D(Ref<Font>& font, const std::string& text,
            vec2 halfSize, float textSize, int32_t* characterCount) {

            *characterCount = 0;

            auto instances = std::vector<vec4>(text.length());

            int32_t index = 0;

            float xOffset = 0.0f;
            float yOffset = 0.0f;

            auto ctext = text.c_str();

            auto nextGlyph = font->GetGlyphUTF8(ctext);

            while (nextGlyph->codepoint) {

                Glyph* glyph = nextGlyph;

                // We have at least one character per line, even if it overshoots the size a bit
                if (xOffset * textSize > 2.0f * halfSize.x) {
                    xOffset = 0.0f;
                    yOffset += font->lineHeight;
                }

                // Just visible characters should be rendered.
                if (glyph->codepoint > 32 && glyph->texArrayIndex < AE_GPU_GLYPH_COUNT) {
                    instances[index].x = (glyph->offset.x + xOffset) * textSize;
                    instances[index].y = (glyph->offset.y + font->ascent + yOffset) * textSize;
                    instances[index].z = (float)glyph->texArrayIndex;
                    index++;
                }

                nextGlyph = font->GetGlyphUTF8(ctext);

                xOffset += glyph->advance + glyph->kern[nextGlyph->codepoint];

            }

            *characterCount = index;

            return instances;

        }

        PipelineConfig TextRenderer::GetPipelineConfig(const Ref<Graphics::FrameBuffer>& frameBuffer, bool threeDimensional) {

            auto shaderConfig = ShaderConfig {
                {"text.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"text.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            Graphics::GraphicsPipelineDesc pipelineDesc {
                .swapChain = device->swapChain,
                .frameBuffer = frameBuffer,
                .assemblyInputInfo = Graphics::Initializers::InitPipelineInputAssemblyStateCreateInfo(
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP),
                .depthStencilInputInfo = Graphics::Initializers::InitPipelineDepthStencilStateCreateInfo(
                    threeDimensional, threeDimensional, VK_COMPARE_OP_LESS_OR_EQUAL)
            };
            // By default, an alpha blending is used
            pipelineDesc.colorBlendAttachment.blendEnable = !threeDimensional;
            pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;

            std::vector<std::string> macros;
            if (threeDimensional)
                macros.push_back("TEXT_3D");
            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}