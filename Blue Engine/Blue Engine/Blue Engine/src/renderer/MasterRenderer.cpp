#include "MasterRenderer.h"

const char* MasterRenderer::shadowVertexPath = "shadow.vsh";
const char* MasterRenderer::shadowFragmentPath = "shadow.fsh";
const char* MasterRenderer::directionalLightVertexPath = "deferred/directional.vsh";
const char* MasterRenderer::directionalLightFragmentPath = "deferred/directional.fsh";
const char* MasterRenderer::skyboxVertexPath = "skybox.vsh";
const char* MasterRenderer::skyboxFragmentPath = "skybox.fsh";
const char* MasterRenderer::postProcessVertexPath = "postprocessing.vsh";
const char* MasterRenderer::postProcessFragmentPath = "postprocessing.fsh";

MasterRenderer::MasterRenderer(const char* shaderDirectory) {

	string directory(shaderDirectory);

	directory += "/";

	rectangleVAO = GenerateRectangleVAO();

	geometryRenderer = new GeometryRenderer();

	shadowRenderer = new ShadowRenderer(
		(directory + string(shadowVertexPath)).c_str(),
		(directory + string(shadowFragmentPath)).c_str());

	directionalLightRenderer = new DirectionalLightRenderer(
		(directory + string(directionalLightVertexPath)).c_str(),
		(directory + string(directionalLightFragmentPath)).c_str());

	skyboxRenderer = new SkyboxRenderer(
		(directory + string(skyboxVertexPath)).c_str(),
		(directory + string(skyboxFragmentPath)).c_str());

	postProcessRenderer = new PostProcessRenderer(
		(directory + string(postProcessVertexPath)).c_str(),
		(directory + string(postProcessFragmentPath)).c_str());

}

void MasterRenderer::RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	target->geometryFramebuffer->Bind();

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	glViewport(0, 0, target->geometryFramebuffer->width, target->geometryFramebuffer->height);

	geometryRenderer->Render(window, target, camera, scene, true);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	target->lightingFramebuffer->Bind();

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindVertexArray(rectangleVAO);

	directionalLightRenderer->Render(window, target, camera, scene, true);

	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);

	if (scene->sky->skybox != nullptr) {
		skyboxRenderer->Render(window, target, camera, scene, true);
	}

	target->lightingFramebuffer->Unbind();

	glDisable(GL_DEPTH_TEST);

	glBindVertexArray(rectangleVAO);

	postProcessRenderer->Render(window, target, camera, scene, true);

	glBindVertexArray(0);

}

void MasterRenderer::RenderTexture(Texture* texture) {

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void MasterRenderer::RenderRectangle(vec3 color) {

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

uint32_t MasterRenderer::GenerateRectangleVAO() {

	uint32_t vao = 0;
	int8_t vertices[] = { -1, -1, 1, -1, -1, 1, 1, 1 };

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glEnableVertexAttribArray(0);

	uint32_t verticesBuffer;
	glGenBuffers(1, &verticesBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, verticesBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_BYTE, false, 0, NULL);

	return vao;

}