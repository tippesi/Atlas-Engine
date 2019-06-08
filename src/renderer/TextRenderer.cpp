#include "TextRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string TextRenderer::vertexPath = "text.vsh";
		std::string TextRenderer::fragmentPath = "text.fsh";

		TextRenderer::TextRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			auto vertexBuffer = new Buffer::VertexBuffer(AE_FLOAT, 3,
				sizeof(vec3), 5000, AE_BUFFER_DYNAMIC_STORAGE);
			vertexArray.AddInstancedComponent(1, vertexBuffer);

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

		}

		void TextRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			return;

		}

		void TextRenderer::Render(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color,
								  float scale, Framebuffer* framebuffer) {

			float width = (float)(framebuffer == nullptr ? viewport->width : framebuffer->width);
			float height = (float)(framebuffer == nullptr ? viewport->height : framebuffer->height);

			vec4 clipArea = vec4(0.0f, 0.0f, width, height);
			vec4 blendArea = vec4(0.0f, 0.0f, width, height);

			RenderOutlined(viewport, font, text, x, y, color, vec4(1.0f), 0.0f, clipArea, blendArea, scale, framebuffer);

		}

		void TextRenderer::Render(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 clipArea,
								  vec4 blendArea, float scale, Framebuffer* framebuffer) {

			RenderOutlined(viewport, font, text, x, y, color, vec4(1.0f), 0.0f, clipArea, blendArea, scale, framebuffer);


		}

		void TextRenderer::RenderOutlined(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor,
										  float outlineScale, float scale, Framebuffer* framebuffer) {

			float width = (float)(framebuffer == nullptr ? viewport->width : framebuffer->width);
			float height = (float)(framebuffer == nullptr ? viewport->height : framebuffer->height);

			vec4 clipArea = vec4(0.0f, 0.0f, width, height);
			vec4 blendArea = vec4(0.0f, 0.0f, width, height);

			RenderOutlined(viewport, font, text, x, y, color, outlineColor, outlineScale, 
				clipArea, blendArea, scale, framebuffer);

		}

		void TextRenderer::RenderOutlined(Viewport* viewport, Font* font, std::string text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
										  vec4 clipArea, vec4 blendArea, float scale, Framebuffer* framebuffer) {

			int32_t characterCount;

			shader.Bind();

			if (framebuffer != nullptr) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, viewport->width, viewport->height);
			}

			glDisable(GL_CULL_FACE);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			float width = (float)(framebuffer == nullptr ? viewport->width : framebuffer->width);
			float height = (float)(framebuffer == nullptr ? viewport->height : framebuffer->height);

			auto instances = CalculateCharacterInstances(font, text, &characterCount);
			vertexArray.GetComponent(1)->SetData(instances.data(), 0, instances.size());

			projectionMatrix->SetValue(glm::ortho(0.0f, width, 0.0f, height));

			textScale->SetValue(scale);
			textOffset->SetValue(vec2(x, y));
			textColor->SetValue(color);

			this->outlineColor->SetValue(outlineColor);
			this->outlineScale->SetValue(outlineScale);

			edgeValue->SetValue(((float)font->edgeValue) / 255.0f);
			smoothness->SetValue(font->smoothness);

			this->clipArea->SetValue(clipArea);
			this->blendArea->SetValue(blendArea);

			font->glyphTexture.Bind(GL_TEXTURE0);

			font->firstGlyphBuffer.BindBase(0);
			font->secondGlyphBuffer.BindBase(1);

			vertexArray.Bind();

			glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, characterCount);

			glDisable(GL_BLEND);

			if (framebuffer != nullptr) {
				framebuffer->Unbind();
			}

			font->firstGlyphBuffer.Unbind();
			font->secondGlyphBuffer.Unbind();

			glEnable(GL_CULL_FACE);

		}

		void TextRenderer::Update() {



		}

		void TextRenderer::GetUniforms() {

			projectionMatrix = shader.GetUniform("pMatrix");
			characterScales = shader.GetUniform("characterScales");
			characterSizes = shader.GetUniform("characterSizes");
			textOffset = shader.GetUniform("textOffset");
			textScale = shader.GetUniform("textScale");
			textColor = shader.GetUniform("textColor");
			outlineColor = shader.GetUniform("outlineColor");
			outlineScale = shader.GetUniform("outlineScale");
			edgeValue = shader.GetUniform("edgeValue");
			smoothness = shader.GetUniform("smoothness");
			clipArea = shader.GetUniform("clipArea");
			blendArea = shader.GetUniform("blendArea");

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