#include "MasterRenderer.h"
#include "helper/GeometryHelper.h"

string MasterRenderer::rectangleVertexPath = "rectangle.vsh";
string MasterRenderer::rectangleFragmentPath = "rectangle.fsh";

MasterRenderer::MasterRenderer() {

	vertexArray = GeometryHelper::GenerateRectangleVertexArray();

	rectangleShader = new Shader();

	rectangleShader->AddStage(VERTEX_SHADER, rectangleVertexPath);
	rectangleShader->AddStage(FRAGMENT_SHADER, rectangleFragmentPath);

	rectangleShader->Compile();

	texturedRectangleShader = new Shader();

	texturedRectangleShader->AddStage(VERTEX_SHADER, rectangleVertexPath);
	texturedRectangleShader->AddStage(FRAGMENT_SHADER, rectangleFragmentPath);

	texturedRectangleShader->AddMacro("TEXTURE");

	texturedRectangleShader->Compile();

	GetUniforms();

	geometryRenderer = new GeometryRenderer();
	terrainRenderer = new TerrainRenderer();
	shadowRenderer = new ShadowRenderer();
	decalRenderer = new DecalRenderer();
	directionalVolumetricRenderer = new DirectionalVolumetricRenderer();
	directionalLightRenderer = new DirectionalLightRenderer();
	pointLightRenderer = new PointLightRenderer();
	skyboxRenderer = new SkyboxRenderer();
	atmosphereRenderer = new AtmosphereRenderer();
	postProcessRenderer = new PostProcessRenderer();
	textRenderer = new TextRenderer();

}

MasterRenderer::~MasterRenderer() {

	vertexArray->DeleteContent();
	delete vertexArray;

	delete rectangleShader;
	delete texturedRectangleShader;

	delete geometryRenderer;
	delete terrainRenderer;
	delete shadowRenderer;
	delete directionalVolumetricRenderer;
	delete directionalLightRenderer;
	delete pointLightRenderer;
	delete skyboxRenderer;
	delete atmosphereRenderer;
	delete postProcessRenderer;

}

void MasterRenderer::RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	shadowRenderer->Render(window, target, camera, scene);

	target->geometryFramebuffer->Bind(true);

	glEnable(GL_CULL_FACE);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	terrainRenderer->Render(window, target, camera, scene);

	geometryRenderer->Render(window, target, camera, scene);

	glEnable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	decalRenderer->Render(window, target, camera, scene);

	glDisable(GL_BLEND);

	vertexArray->Bind();

	directionalVolumetricRenderer->Render(window, target, camera, scene);

	target->lightingFramebuffer->Bind(true);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Additive blending of light volumes
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	directionalLightRenderer->Render(window, target, camera, scene);

	pointLightRenderer->Render(window, target, camera, scene);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	if (scene->sky->skybox != nullptr) {
		skyboxRenderer->Render(window, target, camera, scene);
	}
	else {
		atmosphereRenderer->Render(window, target, camera, scene);
	}

	target->lightingFramebuffer->Unbind();

	glDisable(GL_DEPTH_TEST);

	vertexArray->Bind();

	postProcessRenderer->Render(window, target, camera, scene);

}

void MasterRenderer::RenderTexture(Window* window, Texture* texture, float x, float y, float width, float height,
	bool alphaBlending, Framebuffer* framebuffer) {

	float viewportWidth = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float viewportHeight = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
	vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

	RenderTexture(window, texture, x, y, width, height, clipArea, blendArea, alphaBlending, framebuffer);

}

void MasterRenderer::RenderTexture(Window* window, Texture* texture, float x, float y, float width, float height,
	vec4 clipArea, vec4 blendArea, bool alphaBlending, Framebuffer* framebuffer) {

	vertexArray->Bind();

	texturedRectangleShader->Bind();

	glDisable(GL_CULL_FACE);

	if (framebuffer != nullptr) {
		framebuffer->Bind(true);
	}
	else {
		glViewport(0, 0, window->viewport->width, window->viewport->height);
	}

	if (alphaBlending) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}


	float viewportWidth = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float viewportHeight = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	texturedRectangleProjectionMatrix->SetValue(glm::ortho(0.0f, (float)viewportWidth, 0.0f, (float)viewportHeight));
	texturedRectangleOffset->SetValue(vec2(x, y));
	texturedRectangleScale->SetValue(vec2(width, height));
	texturedRectangleBlendArea->SetValue(blendArea);
	texturedRectangleClipArea->SetValue(clipArea);
	texturedRectangleTexture->SetValue(0);

	texture->Bind(GL_TEXTURE0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	if (alphaBlending) {
		glDisable(GL_BLEND);
	}

	if (framebuffer != nullptr) {
		framebuffer->Unbind();
	}

	glEnable(GL_CULL_FACE);

}

void MasterRenderer::RenderRectangle(Window* window, vec4 color, float x, float y, float width, float height,
	bool alphaBlending, Framebuffer* framebuffer) {

	float viewportWidth = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float viewportHeight = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	if (x > viewportWidth || y > viewportHeight ||
		y + height < 0 || x + width < 0) {
		return;
	}

	vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
	vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

	RenderRectangle(window, color, x, y, width, height, clipArea, blendArea, alphaBlending, framebuffer);

}

void MasterRenderer::RenderRectangle(Window* window, vec4 color, float x, float y, float width, float height,
	vec4 clipArea, vec4 blendArea, bool alphaBlending, Framebuffer* framebuffer) {

	float viewportWidth = (float)(framebuffer == nullptr ? window->viewport->width : framebuffer->width);
	float viewportHeight = (float)(framebuffer == nullptr ? window->viewport->height : framebuffer->height);

	if (x > viewportWidth || y > viewportHeight ||
		y + height < 0 || x + width < 0) {
		return;
	}

	vertexArray->Bind();

	rectangleShader->Bind();

	glDisable(GL_CULL_FACE);

	if (framebuffer != nullptr) {
		framebuffer->Bind(true);
	}
	else {
		glViewport(0, 0, window->viewport->width, window->viewport->height);
	}

	if (alphaBlending) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	rectangleProjectionMatrix->SetValue(glm::ortho(0.0f, (float)viewportWidth, 0.0f, (float)viewportHeight));
	rectangleOffset->SetValue(vec2(x, y));
	rectangleScale->SetValue(vec2(width, height));
	rectangleColor->SetValue(color);
	rectangleBlendArea->SetValue(blendArea);
	rectangleClipArea->SetValue(clipArea);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	if (alphaBlending) {
		glDisable(GL_BLEND);
	}

	if (framebuffer != nullptr) {
		framebuffer->Unbind();
	}

	glEnable(GL_CULL_FACE);

}

void MasterRenderer::GetUniforms() {

	rectangleProjectionMatrix = rectangleShader->GetUniform("pMatrix");
	rectangleOffset = rectangleShader->GetUniform("rectangleOffset");
	rectangleScale = rectangleShader->GetUniform("rectangleScale");
	rectangleColor = rectangleShader->GetUniform("rectangleColor");
	rectangleBlendArea = rectangleShader->GetUniform("rectangleBlendArea");
	rectangleClipArea = rectangleShader->GetUniform("rectangleClipArea");

	texturedRectangleProjectionMatrix = texturedRectangleShader->GetUniform("pMatrix");
	texturedRectangleOffset = texturedRectangleShader->GetUniform("rectangleOffset");
	texturedRectangleScale = texturedRectangleShader->GetUniform("rectangleScale");
	texturedRectangleTexture = texturedRectangleShader->GetUniform("rectangleTexture");
	texturedRectangleBlendArea = texturedRectangleShader->GetUniform("rectangleBlendArea");
	texturedRectangleClipArea = texturedRectangleShader->GetUniform("rectangleClipArea");

}