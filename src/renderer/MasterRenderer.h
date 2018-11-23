#ifndef MASTERRENDERER_H
#define MASTERRENDERER_H

#include "../System.h"

#include "GeometryRenderer.h"
#include "TerrainRenderer.h"
#include "DirectionalShadowRenderer.h"
#include "DirectionalVolumetricRenderer.h"
#include "DirectionalLightRenderer.h"
#include "SkyboxRenderer.h"
#include "PostProcessRenderer.h"
#include "TextRenderer.h"

class MasterRenderer {

public:
	MasterRenderer();

	void RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	void RenderTexture(Texture* texture);

	void RenderRectangle(vec3 color);

	static uint32_t GenerateRectangleVAO();

	TextRenderer* textRenderer;

private:
	GeometryRenderer* geometryRenderer;
	TerrainRenderer* terrainRenderer;
	ShadowRenderer* shadowRenderer;
	DirectionalVolumetricRenderer* directionalVolumetricRenderer;
	DirectionalLightRenderer* directionalLightRenderer;
	SkyboxRenderer* skyboxRenderer;
	PostProcessRenderer* postProcessRenderer;

	uint32_t rectangleVAO;

	static string terrainVertexPath;
	static string terrainTessControlPath;
	static string terrainTessEvalPath;
	static string terrainGeometryPath;
	static string terrainFragmentPath;
	static string shadowVertexPath;
	static string shadowFragmentPath;
	static string volumetricVertexPath;
	static string volumetricFragmentPath;
	static string bilateralBlurVertexPath;
	static string bilateralBlurFragmentPath;
	static string directionalLightVertexPath;
	static string directionalLightFragmentPath;
	static string skyboxVertexPath;
	static string skyboxFragmentPath;
	static string postProcessVertexPath;
	static string postProcessFragmentPath;
	static string textVertexPath;
	static string textFragmentPath;

};

#endif