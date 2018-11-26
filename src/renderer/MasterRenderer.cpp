#include "MasterRenderer.h"
#include "helper/GeometryHelper.h"

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
string MasterRenderer::pointLightVertexPath = "deferred/point.vsh";
string MasterRenderer::pointLightFragmentPath = "deferred/point.fsh";
string MasterRenderer::skyboxVertexPath = "skybox.vsh";
string MasterRenderer::skyboxFragmentPath = "skybox.fsh";
string MasterRenderer::atmosphereVertexPath = "atmosphere.vsh";
string MasterRenderer::atmosphereFragmentPath = "atmosphere.fsh";
string MasterRenderer::postProcessVertexPath = "postprocessing.vsh";
string MasterRenderer::postProcessFragmentPath = "postprocessing.fsh";
string MasterRenderer::textVertexPath = "text.vsh";
string MasterRenderer::textFragmentPath = "text.fsh";

MasterRenderer::MasterRenderer() {

	vertexArray = GeometryHelper::GenerateRectangleVertexArray();

	geometryRenderer = new GeometryRenderer();

	terrainRenderer = new TerrainRenderer(terrainVertexPath,
		terrainTessControlPath,
		terrainTessEvalPath,
		terrainGeometryPath,
		terrainFragmentPath);

	directionalShadowRenderer = new DirectionalShadowRenderer(shadowVertexPath,
		shadowFragmentPath);

	directionalVolumetricRenderer = new DirectionalVolumetricRenderer(volumetricVertexPath, 
		volumetricFragmentPath, 
		bilateralBlurVertexPath, 
		bilateralBlurFragmentPath);

	directionalLightRenderer = new DirectionalLightRenderer(directionalLightVertexPath, 
		directionalLightFragmentPath);

	pointLightRenderer = new PointLightRenderer(pointLightVertexPath,
		pointLightFragmentPath);

	skyboxRenderer = new SkyboxRenderer(skyboxVertexPath,
		skyboxFragmentPath);

	atmosphereRenderer = new AtmosphereRenderer(atmosphereVertexPath,
		atmosphereFragmentPath);

	postProcessRenderer = new PostProcessRenderer(postProcessVertexPath,
		postProcessFragmentPath);

	textRenderer = new TextRenderer(textVertexPath,
		textFragmentPath);

}

void MasterRenderer::RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	directionalShadowRenderer->Render(window, target, camera, scene, true);

	target->geometryFramebuffer->Bind(true);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	terrainRenderer->Render(window, target, camera, scene, true);

	geometryRenderer->Render(window, target, camera, scene, true);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	vertexArray->Bind();

	directionalVolumetricRenderer->Render(window, target, camera, scene, true);

	target->lightingFramebuffer->Bind(true);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Additive blending of light volumes
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);

	directionalLightRenderer->Render(window, target, camera, scene, true);

	pointLightRenderer->Render(window, target, camera, scene, true);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	if (scene->sky->skybox != nullptr) {
		//skyboxRenderer->Render(window, target, camera, scene, true);
	}

	atmosphereRenderer->Render(window, target, camera, scene, true);

	target->lightingFramebuffer->Unbind();

	glDisable(GL_DEPTH_TEST);

	vertexArray->Bind();

	postProcessRenderer->Render(window, target, camera, scene, true);

}

void MasterRenderer::RenderTexture(Texture* texture) {

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void MasterRenderer::RenderRectangle(vec3 color) {

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

MasterRenderer::~MasterRenderer() {

	delete geometryRenderer;
	delete terrainRenderer;
	delete directionalShadowRenderer;
	delete directionalVolumetricRenderer;
	delete directionalLightRenderer;
	delete pointLightRenderer;
	delete skyboxRenderer;
	delete atmosphereRenderer;
	delete postProcessRenderer;

	vertexArray->DeleteContent();
	delete vertexArray;

}