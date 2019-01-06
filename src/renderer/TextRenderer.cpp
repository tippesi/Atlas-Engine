#include "TextRenderer.h"
#include "helper/GeometryHelper.h"

string TextRenderer::vertexPath = "text.vsh";
string TextRenderer::fragmentPath = "text.fsh";

TextRenderer::TextRenderer() {

	vertexArray = GeometryHelper::GenerateRectangleVertexArray();

	// Hard coded maximum of 1000 character per draw call
	VertexBuffer* vertexBuffer = new VertexBuffer(GL_FLOAT, 3, sizeof(vec3), 1000, BUFFER_DYNAMIC_STORAGE |
		BUFFER_TRIPLE_BUFFERING | BUFFER_MAP_WRITE | BUFFER_IMMUTABLE);
	vertexBuffer->Map();
	vertexArray->AddInstancedComponent(1, vertexBuffer);

	shader = new Shader();

	shader->AddStage(VERTEX_SHADER, vertexPath);
	shader->AddStage(FRAGMENT_SHADER, fragmentPath);

	shader->Compile();

	GetUniforms();

}

void TextRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	return;

}

void TextRenderer::Render(Window* window, Font* font, string text, float x, float y, vec4 color,
	float scale, bool alphaBlending, Framebuffer* framebuffer) {

	float width = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float height = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	vec4 clipArea = vec4(0.0f, 0.0f, width, height);
	vec4 blendArea = vec4(0.0f, 0.0f, width, height);

	Render(window, font, text, x, y, color, clipArea, blendArea, scale, alphaBlending, framebuffer);

}

void TextRenderer::Render(Window* window, Font* font, string text, float x, float y, vec4 color, vec4 clipArea,
	vec4 blendArea, float scale, bool alphaBlending, Framebuffer* framebuffer) {
	
	int32_t characterCount;

	float width = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float height = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	if (x > width || y > height)
		return;

	shader->Bind();

	outline->SetValue(false);

	if (framebuffer != nullptr) {
		framebuffer->Bind(true);
	}
	else {
		glViewport(0, 0, window->viewport->width, window->viewport->height);
	}

	glDisable(GL_CULL_FACE);

	if (alphaBlending) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	auto instances = CalculateCharacterInstances(font, text, &characterCount);

	glyphsTexture->SetValue(0);

	projectionMatrix->SetValue(glm::ortho(0.0f, width, 0.0f, height));

	textScale->SetValue(scale);
	textOffset->SetValue(vec2(x, y));
	textColor->SetValue(color);

	characterScales->SetValue(font->characterScales, FONT_CHARACTER_COUNT);
	characterSizes->SetValue(font->characterSizes, FONT_CHARACTER_COUNT);
	pixelDistanceScale->SetValue(font->pixelDistanceScale);
	edgeValue->SetValue(font->edgeValue);

	this->clipArea->SetValue(clipArea);
	this->blendArea->SetValue(blendArea);

	//vertexArray->GetComponent(1)->Bind();
	//glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(vec3), NULL, GL_STATIC_DRAW);
	//glBufferSubData(GL_ARRAY_BUFFER, 0, instances.size() * sizeof(vec3), glm::value_ptr(instances[0]));
	// vertexArray->GetComponent(1)->SetData(&instances.data()[0], 0, characterCount);

	font->glyphsTexture->Bind(GL_TEXTURE0);

	vertexArray->Bind();

	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, characterCount);

	if (alphaBlending) {
		glDisable(GL_BLEND);
	}

	if (framebuffer != nullptr) {
		framebuffer->Unbind();
	}

	glEnable(GL_CULL_FACE);

	vertexArray->GetComponent(1)->Increment();

}

void TextRenderer::RenderOutlined(Window* window, Font* font, string text, float x, float y, vec4 color, vec4 outlineColor, 
	float outlineScale, float scale, bool alphaBlending, Framebuffer* framebuffer) {

	float width = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float height = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	vec4 clipArea = vec4(0.0f, 0.0f, width, height);
	vec4 blendArea = vec4(0.0f, 0.0f, width, height);

	RenderOutlined(window, font, text, x, y, color, outlineColor, outlineScale, clipArea, blendArea,
		scale, alphaBlending, framebuffer);

}

void TextRenderer::RenderOutlined(Window* window, Font* font, string text, float x, float y, vec4 color, vec4 outlineColor, float outlineScale,
	vec4 clipArea, vec4 blendArea, float scale, bool alphaBlending, Framebuffer* framebuffer) {

	/*
	int32_t characterCount;

	shader->Bind();

	outline->SetValue(true);

	if (framebuffer != nullptr) {
		framebuffer->Bind(true);
	}
	else {
		glViewport(0, 0, window->viewport->width, window->viewport->height);
	}

	glDisable(GL_CULL_FACE);

	if (alphaBlending) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	float width = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float height = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	auto instances = CalculateCharacterInstances(font, text, &characterCount);

	glyphsTexture->SetValue(0);

	projectionMatrix->SetValue(glm::ortho(0.0f, width, 0.0f, height));

	textScale->SetValue(scale);
	textOffset->SetValue(vec2(x, y));
	textColor->SetValue(color);

	this->outlineColor->SetValue(outlineColor);
	this->outlineScale->SetValue(outlineScale);

	characterScales->SetValue(font->characterScales, FONT_CHARACTER_COUNT);
	characterSizes->SetValue(font->characterSizes, FONT_CHARACTER_COUNT);
	pixelDistanceScale->SetValue(font->pixelDistanceScale);
	edgeValue->SetValue(font->edgeValue);

	this->clipArea->SetValue(clipArea);
	this->blendArea->SetValue(blendArea);

	vertexArray->GetComponent(1)->SetData(instances.data(), characterCount);

	font->glyphsTexture->Bind(GL_TEXTURE0);

	vertexArray->Bind();

	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, characterCount);

	if (alphaBlending) {
		glDisable(GL_BLEND);
	}

	if (framebuffer != nullptr) {
		framebuffer->Unbind();
	}

	glEnable(GL_CULL_FACE);
	 */

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
	clipArea = shader->GetUniform("clipArea");
	blendArea = shader->GetUniform("blendArea");

}

vector<vec3> TextRenderer::CalculateCharacterInstances(Font* font, string text, int32_t* characterCount) {

	*characterCount = 0;

	auto instances = vector<vec3>(text.length());

	int32_t index = 0;

	float xOffset = 0.0f;

	for (uint32_t i = 0; i < text.length(); i++) {

		char& character = text[i];
		Glyph* glyph = font->GetGlyph(character);

		// Just visible characters should be rendered.
		if ((uint8_t)character > 32) {
			instances[index].x = glyph->offset.x + xOffset;
			instances[index].y = glyph->offset.y + font->ascent;
			instances[index].z = (float)glyph->texArrayIndex;
			vertexArray->GetComponent(1)->SetDataMapped(&instances[index]);
			index++;
		}

		xOffset += glyph->advance + glyph->kern[(uint8_t)text[i + 1]];
		
	}

	*characterCount = index;

	return instances;

}