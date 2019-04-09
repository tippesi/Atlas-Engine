#include "TextRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string TextRenderer::vertexPath = "text.vsh";
		std::string TextRenderer::fragmentPath = "text.fsh";

		TextRenderer::TextRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			auto vertexBuffer = new Buffer::VertexBuffer(AE_FLOAT, 3, sizeof(vec3), 5000);
			vertexArray.AddInstancedComponent(1, vertexBuffer);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void TextRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			return;

		}

		void TextRenderer::Render(Window* window, Font* font, std::string text, float x, float y, vec4 color,
								  float scale, bool alphaBlending, Framebuffer* framebuffer) {

			float width = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float height = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			vec4 clipArea = vec4(0.0f, 0.0f, width, height);
			vec4 blendArea = vec4(0.0f, 0.0f, width, height);

			Render(window, font, text, x, y, color, clipArea, blendArea, scale, alphaBlending, framebuffer);

		}

		void TextRenderer::Render(Window* window, Font* font, std::string text, float x, float y, vec4 color, vec4 clipArea,
								  vec4 blendArea, float scale, bool alphaBlending, Framebuffer* framebuffer) {

			int32_t characterCount;

			float width = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float height = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			if (x > width || y > height)
				return;

			shader.Bind();

			outline->SetValue(false);

			if (framebuffer != nullptr) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, window->viewport.width, window->viewport.height);
			}

			glDisable(GL_CULL_FACE);

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			auto instances = CalculateCharacterInstances(font, text, &characterCount);
			vertexArray.GetComponent(1)->SetData(instances.data(), 0, instances.size());

			glyphsTexture->SetValue(0);

			projectionMatrix->SetValue(glm::ortho(0.0f, width, 0.0f, height));

			textScale->SetValue(scale);
			textOffset->SetValue(vec2(x, y));
			textColor->SetValue(color);

			pixelDistanceScale->SetValue(font->pixelDistanceScale);
			edgeValue->SetValue(font->edgeValue);

			this->clipArea->SetValue(clipArea);
			this->blendArea->SetValue(blendArea);

			font->glyphTexture->Bind(GL_TEXTURE0);

			font->firstGlyphBuffer->BindBase(0);
			font->secondGlyphBuffer->BindBase(1);

			vertexArray.Bind();

			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, characterCount);

			if (alphaBlending) {
				glDisable(GL_BLEND);
			}

			if (framebuffer != nullptr) {
				framebuffer->Unbind();
			}

			font->firstGlyphBuffer->Unbind();
			font->secondGlyphBuffer->Unbind();

			glEnable(GL_CULL_FACE);

		}

		void TextRenderer::RenderOutlined(Window* window, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor,
										  float outlineScale, float scale, bool alphaBlending, Framebuffer* framebuffer) {

			float width = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float height = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			vec4 clipArea = vec4(0.0f, 0.0f, width, height);
			vec4 blendArea = vec4(0.0f, 0.0f, width, height);

			RenderOutlined(window, font, text, x, y, color, outlineColor, outlineScale, clipArea, blendArea,
						   scale, alphaBlending, framebuffer);

		}

		void TextRenderer::RenderOutlined(Window* window, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
										  vec4 clipArea, vec4 blendArea, float scale, bool alphaBlending, Framebuffer* framebuffer) {

			int32_t characterCount;

			shader.Bind();

			outline->SetValue(true);

			if (framebuffer != nullptr) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, window->viewport.width, window->viewport.height);
			}

			glDisable(GL_CULL_FACE);

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			float width = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float height = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			auto instances = CalculateCharacterInstances(font, text, &characterCount);
			vertexArray.GetComponent(1)->SetData(instances.data(), 0, instances.size());

			glyphsTexture->SetValue(0);

			projectionMatrix->SetValue(glm::ortho(0.0f, width, 0.0f, height));

			textScale->SetValue(scale);
			textOffset->SetValue(vec2(x, y));
			textColor->SetValue(color);

			this->outlineColor->SetValue(outlineColor);
			this->outlineScale->SetValue(outlineScale);

			pixelDistanceScale->SetValue(font->pixelDistanceScale);
			edgeValue->SetValue(font->edgeValue);

			this->clipArea->SetValue(clipArea);
			this->blendArea->SetValue(blendArea);

			font->glyphTexture->Bind(GL_TEXTURE0);

			font->firstGlyphBuffer->BindBase(0);
			font->secondGlyphBuffer->BindBase(1);

			vertexArray.Bind();

			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, characterCount);

			if (alphaBlending) {
				glDisable(GL_BLEND);
			}

			if (framebuffer != nullptr) {
				framebuffer->Unbind();
			}

			font->firstGlyphBuffer->Unbind();
			font->secondGlyphBuffer->Unbind();

			glEnable(GL_CULL_FACE);

		}

		void TextRenderer::Update() {



		}

		void TextRenderer::GetUniforms() {

			glyphsTexture = shader.GetUniform("glyphsTexture");
			projectionMatrix = shader.GetUniform("pMatrix");
			characterScales = shader.GetUniform("characterScales");
			characterSizes = shader.GetUniform("characterSizes");
			textOffset = shader.GetUniform("textOffset");
			textScale = shader.GetUniform("textScale");
			textColor = shader.GetUniform("textColor");
			outline = shader.GetUniform("outline");
			outlineColor = shader.GetUniform("outlineColor");
			outlineScale = shader.GetUniform("outlineScale");
			pixelDistanceScale = shader.GetUniform("pixelDistanceScale");
			edgeValue = shader.GetUniform("edgeValue");
			clipArea = shader.GetUniform("clipArea");
			blendArea = shader.GetUniform("blendArea");

			// Can be removed later on when we raise the version requirements to 4.2
			uint32_t firstBufferIndex = glGetUniformBlockIndex(shader.GetID(), "UBO1");
			uint32_t secondBufferIndex = glGetUniformBlockIndex(shader.GetID(), "UBO2");

			glUniformBlockBinding(shader.GetID(), firstBufferIndex, 0);
			glUniformBlockBinding(shader.GetID(), secondBufferIndex, 1);

		}

		std::vector<vec3> TextRenderer::CalculateCharacterInstances(Font* font, std::string text, int32_t* characterCount) {

			*characterCount = 0;

			auto instances = std::vector<vec3>(text.length());

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

	}

}