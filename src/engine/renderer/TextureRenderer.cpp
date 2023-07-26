#include "TextureRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

    namespace Renderer {

        void TextureRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

        }

        void TextureRenderer::RenderTexture2D(Graphics::CommandList* commandList, Viewport* viewport,
            Texture::Texture2D* texture, float x, float y, float width, float height,
            float rotation, float brightness, bool alphaBlending, bool invert) {


            float viewportWidth = (float)viewport->width;
            float viewportHeight = (float)viewport->height;

            vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
            vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

            RenderTexture2D(commandList, viewport, texture, x, y, width, height,
                clipArea, blendArea, rotation, brightness, alphaBlending, invert);


        }

        void TextureRenderer::RenderTexture2D(Graphics::CommandList* commandList, Viewport* viewport,
            Texture::Texture2D* texture, float x, float y, float width, float height, vec4 clipArea,
            vec4 blendArea, float rotation, float brightness, bool alphaBlending, bool invert) {

            Draw(commandList, viewport, texture, 0.0f, x, y, width, height, clipArea, blendArea,
                rotation, brightness, alphaBlending, invert, "TEXTURE2D");

        }

        void TextureRenderer::RenderTexture2DArray(Graphics::CommandList* commandList, Viewport* viewport,
            Texture::Texture2DArray* texture, int32_t depth, float x, float y, float width, float height,
            bool alphaBlending, bool invert) {

            float viewportWidth = (float)(viewport->width);
            float viewportHeight = (float)(viewport->height);

            vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
            vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

            RenderTexture2DArray(commandList, viewport, texture, depth, x, y, width, height,
                clipArea, blendArea, alphaBlending, invert);

        }

        void TextureRenderer::RenderTexture2DArray(Graphics::CommandList* commandList, Viewport* viewport,
            Texture::Texture2DArray* texture, int32_t depth, float x, float y, float width, float height,
            vec4 clipArea, vec4 blendArea, bool alphaBlending, bool invert) {

            Draw(commandList, viewport, texture, depth, x, y, width, height,
                clipArea, blendArea, 0.0f, 1.0f, alphaBlending, invert, "TEXTURE2D_ARRAY");

        }

        void TextureRenderer::RenderTexture3D(Graphics::CommandList* commandList, Viewport* viewport,
            Texture::Texture3D* texture, float depth, float x, float y, float width, float height,
            bool alphaBlending, bool invert) {

            float viewportWidth = (float)(viewport->width);
            float viewportHeight = (float)(viewport->height);

            vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
            vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

            RenderTexture3D(commandList, viewport, texture, depth, x, y, width, height,
                clipArea, blendArea, alphaBlending, invert);

        }

        void TextureRenderer::RenderTexture3D(Graphics::CommandList* commandList, Viewport* viewport,
            Texture::Texture3D* texture, float depth, float x, float y, float width, float height,
            vec4 clipArea, vec4 blendArea, bool alphaBlending, bool invert) {

            Draw(commandList, viewport, texture, depth, x, y, width, height,
                clipArea, blendArea, 0.0f, 1.0f, alphaBlending, invert, "TEXTURE3D");

        }

        void TextureRenderer::Draw(Graphics::CommandList* commandList, Viewport* viewport, Texture::Texture* texture,
            float depth, float x, float y, float width, float height, vec4 clipArea, vec4 blendArea,
            float rotation, float brightness, bool alphaBlending, bool invert, const std::string& macro) {

            std::vector<std::string> macros = { macro };

            // Not supported right now
            if (alphaBlending) {
                macros.push_back("ALPHA_BLENDING");
            }

            auto pipelineConfig = GeneratePipelineConfig(nullptr, macros);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            vertexArray.Bind(commandList);

            auto projectionMatrix = glm::ortho(0.0f, (float)viewport->width, 0.0f, (float)viewport->height);
            PushConstants constants = {
                .pMatrix = projectionMatrix,
                .blendArea = blendArea,
                .clipArea = clipArea,
                .offset = vec2(x, y),
                .scale = vec2(width, height),
                .invert = invert ? 1 : 0,
                .depth = float(depth),
                .rotation = rotation,
                .brightness = brightness
            };
            commandList->PushConstants("constants", &constants);

            commandList->BindImage(texture->image, texture->sampler, 3, 0);

            commandList->Draw(4);

        }

        PipelineConfig TextureRenderer::GeneratePipelineConfig(const Ref<Graphics::FrameBuffer>& frameBuffer,
            const std::vector<std::string>& macros) {

            auto shaderConfig = ShaderConfig {
                {"rectangle.vsh", VK_SHADER_STAGE_VERTEX_BIT},
                {"rectangle.fsh", VK_SHADER_STAGE_FRAGMENT_BIT},
            };
            Graphics::GraphicsPipelineDesc pipelineDesc {
                .swapChain = device->swapChain,
                .frameBuffer = frameBuffer,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
                .assemblyInputInfo = Graphics::Initializers::InitPipelineInputAssemblyStateCreateInfo(
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP),
                .depthStencilInputInfo = Graphics::Initializers::InitPipelineDepthStencilStateCreateInfo(
                    false, false, VK_COMPARE_OP_LESS_OR_EQUAL),
                .rasterizer = Graphics::Initializers::InitPipelineRasterizationStateCreateInfo(
                    VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE)
            };
            return PipelineConfig(shaderConfig, pipelineDesc, macros);

        }

    }

}