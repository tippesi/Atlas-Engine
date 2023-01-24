#include "TextureRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

        void TextureRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

		}

		void TextureRenderer::RenderTexture2D(Graphics::CommandList* commandList, Viewport* viewport,
            Texture::Texture2D* texture, float x, float y, float width, float height, bool alphaBlending, bool invert) {


			float viewportWidth = (float)viewport->width;
			float viewportHeight = (float)viewport->height;

			vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
			vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

			RenderTexture2D(commandList, viewport, texture, x, y, width, height,
                clipArea, blendArea, alphaBlending, invert);


		}

		void TextureRenderer::RenderTexture2D(Graphics::CommandList* commandList, Viewport* viewport,
            Texture::Texture2D* texture, float x, float y, float width, float height,
			vec4 clipArea, vec4 blendArea, bool alphaBlending, bool invert) {

            /*
			vertexArray.Bind();
			texture2DShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer)
				framebuffer->Bind();

			glViewport(0, 0, viewport->width, viewport->height);

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			auto projectionMatrix = glm::ortho(0.0f, (float)viewport->width, 0.0f, (float)viewport->height);
			texture2DShader.GetUniform("pMatrix")->SetValue(projectionMatrix);
			texture2DShader.GetUniform("offset")->SetValue(vec2(x, y));
			texture2DShader.GetUniform("scale")->SetValue(vec2(width, height));
			texture2DShader.GetUniform("blendArea")->SetValue(blendArea);
			texture2DShader.GetUniform("clipArea")->SetValue(clipArea);
			texture2DShader.GetUniform("invert")->SetValue(invert);

			texture->Bind(0);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (alphaBlending)
				glDisable(GL_BLEND);

			if (framebuffer)
				framebuffer->Unbind();

			glEnable(GL_CULL_FACE);
             */

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

            /*
			vertexArray.Bind();
			texture2DArrayShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, viewport->width, viewport->height);
			}

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}


			float viewportWidth = (float)(!framebuffer ? viewport->width : framebuffer->width);
			float viewportHeight = (float)(!framebuffer ? viewport->height : framebuffer->height);

			auto projectionMatrix = glm::ortho(0.0f, (float)viewport->width, 0.0f, (float)viewport->height);
			texture2DArrayShader.GetUniform("pMatrix")->SetValue(projectionMatrix);
			texture2DArrayShader.GetUniform("offset")->SetValue(vec2(x, y));
			texture2DArrayShader.GetUniform("scale")->SetValue(vec2(width, height));
			texture2DArrayShader.GetUniform("blendArea")->SetValue(blendArea);
			texture2DArrayShader.GetUniform("clipArea")->SetValue(clipArea);
			texture2DArrayShader.GetUniform("invert")->SetValue(invert);
			texture2DArrayShader.GetUniform("depth")->SetValue(float(depth));

			texture->Bind(0);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (alphaBlending) {
				glDisable(GL_BLEND);
			}

			if (framebuffer) {
				framebuffer->Unbind();
			}

			glEnable(GL_CULL_FACE);
             */

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

            /*
			vertexArray.Bind();
			texture3DShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, viewport->width, viewport->height);
			}

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}


			float viewportWidth = (float)(!framebuffer ? viewport->width : framebuffer->width);
			float viewportHeight = (float)(!framebuffer ? viewport->height : framebuffer->height);

			auto projectionMatrix = glm::ortho(0.0f, (float)viewport->width, 0.0f, (float)viewport->height);
			texture3DShader.GetUniform("pMatrix")->SetValue(projectionMatrix);
			texture3DShader.GetUniform("offset")->SetValue(vec2(x, y));
			texture3DShader.GetUniform("scale")->SetValue(vec2(width, height));
			texture3DShader.GetUniform("blendArea")->SetValue(blendArea);
			texture3DShader.GetUniform("clipArea")->SetValue(clipArea);
			texture3DShader.GetUniform("invert")->SetValue(invert);
			texture3DShader.GetUniform("depth")->SetValue(depth);

			texture->Bind(0);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (alphaBlending) {
				glDisable(GL_BLEND);
			}

			if (framebuffer) {
				framebuffer->Unbind();
			}

			glEnable(GL_CULL_FACE);
             */

		}

        PipelineConfig TextureRenderer::GeneratePipelineConfig(const Ref<Graphics::FrameBuffer>& frameBuffer,
            const std::string& macro) {

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
            return PipelineConfig(shaderConfig, pipelineDesc, {macro});

        }

	}

}