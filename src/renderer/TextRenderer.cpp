#include "TextRenderer.h"
#include "MasterRenderer.h"

TextRenderer::TextRenderer(string vertexSource, string fragmentSource) {

	vao = MasterRenderer::GenerateRectangleVAO();

	vboLength = 100;

	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vboLength * sizeof(vec3), NULL, GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribDivisor(1, 1);

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSource);

	shader->Compile();

	GetUniforms();

}

void TextRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	return;

}

void TextRenderer::Render(Window* window, Font* font, string text, int32_t x, int32_t y, float scale, Framebuffer* framebuffer) {

	shader->Bind();

	if (framebuffer != nullptr) {
		framebuffer->Bind(true);
	}

	glDisable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float width = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float height = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	vec3* instances = CalculateCharacterInstances(font, text);

	glyphsTexture->SetValue(0);

	projectionMatrix->SetValue(glm::ortho(0.0f, width, 0.0f, height));

	textScale->SetValue(scale);
	textOffset->SetValue(vec2((float)x, (float)y));

	characterScales->SetValue(font->characterScales, FONT_CHARACTER_COUNT);
	characterOffsets->SetValue(font->characterOffsets, FONT_CHARACTER_COUNT);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	if (text.length() > vboLength) {
		vboLength = text.length();
		glBufferData(GL_ARRAY_BUFFER, vboLength * sizeof(vec3), glm::value_ptr(instances[0]), GL_STATIC_DRAW);
	}
	else {
		glBufferSubData(GL_ARRAY_BUFFER, 0, text.length() * sizeof(vec3), glm::value_ptr(instances[0]));
	}

	font->glyphsTexture->Bind(GL_TEXTURE0);

	glBindVertexArray(vao);

	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, text.length());

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

}

void TextRenderer::RenderOutlined(Window* window, Font* font, string text, int32_t x, int32_t y, float scale, Framebuffer* framebuffer) {

	shader->Bind();

	if (framebuffer != nullptr) {
		framebuffer->Bind(true);
	}

}

void TextRenderer::GetUniforms() {

	glyphsTexture = shader->GetUniform("glyphsTexture");
	projectionMatrix = shader->GetUniform("pMatrix");
	characterScales = shader->GetUniform("characterScales");
	characterOffsets = shader->GetUniform("characterOffsets");
	textOffset = shader->GetUniform("textOffset");
	textScale = shader->GetUniform("textScale");

}

vec3* TextRenderer::CalculateCharacterInstances(Font* font, string text) {

	vec3* instances = new vec3[text.length()];

	int32_t index = 0;

	float xOffset = 0.0f;

	for (int32_t i = 0; i < text.length(); i++) {

		char& character = text[i];
		Glyph* glyph = font->GetGlyph(character);

		instances[index].x = xOffset;
		instances[index].y = 0.0f;
		instances[index].z = (float)character;
		index++;

		xOffset += glyph->advance + glyph->kern[(uint8_t)text[i + 1]];
		
	}

	return instances;

}