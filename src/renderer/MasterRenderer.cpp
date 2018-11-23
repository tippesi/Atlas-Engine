#include "MasterRenderer.h"

string MasterRenderer::terrainVertexPath = "terrain/terrain.vsh";
string MasterRenderer::terrainTessControlPath = "terrain/terrain.tcsh";
string MasterRenderer::terrainTessEvalPath = "terrain/terrain.tesh";
string MasterRenderer::terrainGeometryPath = "terrain/terrain.gsh";
string MasterRenderer::terrainFragmentPath = "terrain/terrain.fsh";
string MasterRenderer::shadowVertexPath = "shadowmapping.vsh";
string MasterRenderer::shadowFragmentPath = "shadowmapping.fsh";
string MasterRenderer::volumetricVertexPath = "volumetric.vsh";
string MasterRenderer::volumetricFragmentPath = "volumetric.fsh";
string MasterRenderer::bilateralBlurVertexPath = "bilateralBlur.vsh";
string MasterRenderer::bilateralBlurFragmentPath = "bilateralBlur.fsh";
string MasterRenderer::directionalLightVertexPath = "deferred/directional.vsh";
string MasterRenderer::directionalLightFragmentPath = "deferred/directional.fsh";
string MasterRenderer::skyboxVertexPath = "skybox.vsh";
string MasterRenderer::skyboxFragmentPath = "skybox.fsh";
string MasterRenderer::postProcessVertexPath = "postprocessing.vsh";
string MasterRenderer::postProcessFragmentPath = "postprocessing.fsh";
string MasterRenderer::textVertexPath = "text.vsh";
string MasterRenderer::textFragmentPath = "text.fsh";

MasterRenderer::MasterRenderer() {

	rectangleVAO = GenerateRectangleVAO();

	geometryRenderer = new GeometryRenderer();

	terrainRenderer = new TerrainRenderer(terrainVertexPath,
		terrainTessControlPath,
		terrainTessEvalPath,
		terrainGeometryPath,
		terrainFragmentPath);

	shadowRenderer = new ShadowRenderer(shadowVertexPath,
		shadowFragmentPath);

	directionalVolumetricRenderer = new DirectionalVolumetricRenderer(volumetricVertexPath, 
		volumetricFragmentPath, 
		bilateralBlurVertexPath, 
		bilateralBlurFragmentPath);

	directionalLightRenderer = new DirectionalLightRenderer(directionalLightVertexPath, 
		directionalLightFragmentPath);

	skyboxRenderer = new SkyboxRenderer(skyboxVertexPath,
		skyboxFragmentPath);

	postProcessRenderer = new PostProcessRenderer(postProcessVertexPath,
		postProcessFragmentPath);

	textRenderer = new TextRenderer(textVertexPath,
		textFragmentPath);

}

void MasterRenderer::RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	shadowRenderer->Render(window, target, camera, scene, true);

	target->geometryFramebuffer->Bind(true);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	terrainRenderer->Render(window, target, camera, scene, true);

	geometryRenderer->Render(window, target, camera, scene, true);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	glBindVertexArray(rectangleVAO);

	directionalVolumetricRenderer->Render(window, target, camera, scene, true);

	target->lightingFramebuffer->Bind();

	directionalLightRenderer->Render(window, target, camera, scene, true);

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