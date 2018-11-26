#include "TextRenderer.h"
#include "helper/GeometryHelper.h"

TextRenderer::TextRenderer(string vertexSource, string fragmentSource) {

	vertexArray = GeometryHelper::GenerateRectangleVertexArray();

	VertexBuffer* vertexBuffer = new VertexBuffer(GL_ARRAY_BUFFER, GL_FLOAT, 3);
	vertexArray->AddInstancedComponent(1, vertexBuffer);

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSource);

	shader->Compile();

	GetUniforms();

}

void TextRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	return;

}

void TextRenderer::Render(Window* window, Font* font, string text, int32_t x, int32_t y, vec4 color,
	float scale, bool alphaBlending, Framebuffer* framebuffer) {

	int32_t characterCount;

	shader->Bind();

	outline->SetValue(false);

	if (framebuffer != nullptr) {
		framebuffer->Bind(true);
	}

	glDisable(GL_CULL_FACE);

	if (alphaBlending) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	float width = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float height = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	vec3* instances = CalculateCharacterInstances(font, text, &characterCount);

	glyphsTexture->SetValue(0);

	projectionMatrix->SetValue(glm::ortho(0.0f, width, 0.0f, height));

	textScale->SetValue(scale);
	textOffset->SetValue(vec2((float)x, (float)y));
	textColor->SetValue(color);

	characterScales->SetValue(font->characterScales, FONT_CHARACTER_COUNT);
	characterSizes->SetValue(font->characterSizes, FONT_CHARACTER_COUNT);
	pixelDistanceScale->SetValue(font->pixelDistanceScale);
	edgeValue->SetValue(font->edgeValue);

	vertexArray->GetComponent(1)->SetData(instances, characterCount);

	font->glyphsTexture->Bind(GL_TEXTURE0);

	vertexArray->Bind();

	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, characterCount);

	if (alphaBlending) {
		glDisable(GL_BLEND);
	}

	glEnable(GL_CULL_FACE);

}

void TextRenderer::RenderOutlined(Window* window, Font* font, string text, int32_t x, int32_t y, vec4 color, vec4 outlineColor, 
	float outlineScale, float scale, bool alphaBlending, Framebuffer* framebuffer) {

	int32_t characterCount;

	shader->Bind();

	outline->SetValue(true);

	if (framebuffer != nullptr) {
		framebuffer->Bind(true);
	}

	glDisable(GL_CULL_FACE);

	if (alphaBlending) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	float width = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float height = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	vec3* instances = CalculateCharacterInstances(font, text, &characterCount);

	glyphsTexture->SetValue(0);

	projectionMatrix->SetValue(glm::ortho(0.0f, width, 0.0f, height));

	textScale->SetValue(scale);
	textOffset->SetValue(vec2((float)x, (float)y));
	textColor->SetValue(color);

	this->outlineColor->SetValue(outlineColor);
	this->outlineScale->SetValue(outlineScale);

	characterScales->SetValue(font->characterScales, FONT_CHARACTER_COUNT);
	characterSizes->SetValue(font->characterSizes, FONT_CHARACTER_COUNT);
	pixelDistanceScale->SetValue(font->pixelDistanceScale);
	edgeValue->SetValue(font->edgeValue);

	vertexArray->GetComponent(1)->SetData(instances, characterCount);

	font->glyphsTexture->Bind(GL_TEXTURE0);

	vertexArray->Bind();

	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, characterCount);

	if (alphaBlending) {
		glDisable(GL_BLEND);
	}

	glEnable(GL_CULL_FACE);

}

void TextRenderer::GetUniforms() {

	glyphsTexture = shader->GetUniform("glyphsTexture");
	projectionMatrix = shader->GetUniform("pMatrix");
	characterScales = shader->GetUniform("characterScales");
	characterSizes = shader->GetUniform("characterSizes");
	textOffset = shader->GetUniform("textOffset");
	textScale = shader->GetUniform("textScale");
	textColor = shader->GetUniform("textColor");
	outline = shader->GetUniform("outline");
	outlineColor = shader->GetUniform("outlineColor");
	outlineScale = shader->GetUniform("outlineScale");
	pixelDistanceScale = shader->GetUniform("pixelDistanceScale");
	edgeValue = shader->GetUniform("edgeValue");

}

vec3* TextRenderer::CalculateCharacterInstances(Font* font, string text, int32_t* characterCount) {

	*characterCount = 0;

	vec3* instances = new vec3[text.length()];

	int32_t index = 0;

	float xOffset = 0.0f;

	for (uint32_t i = 0; i < text.length(); i++) {

		char& character = text[i];
		Glyph* glyph = font->GetGlyph(character);

		// Just visible characters should be rendered.
		if ((uint8_t)character > 32) {
			instances[index].x = xOffset;
			instances[index].y = glyph->offset.y + font->ascent;
			instances[index].z = (float)character;
			index++;
		}

		xOffset += glyph->advance + glyph->kern[(uint8_t)text[i + 1]];
		
	}

	*characterCount = index;

	return instances;

}