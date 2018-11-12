#ifndef MASTERRENDERER_H
#define MASTERRENDERER_H

#include "../System.h"

#include "GeometryRenderer.h"
#include "ShadowRenderer.h"
#include "DirectionalVolumetricRenderer.h"
#include "DirectionalLightRenderer.h"
#include "SkyboxRenderer.h"
#include "PostProcessRenderer.h"

class MasterRenderer {

public:
	MasterRenderer();

	void RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	void RenderTexture(Texture* texture);

	void RenderRectangle(vec3 color);

	static uint32_t GenerateRectangleVAO();

private:
	GeometryRenderer* geometryRenderer;
	ShadowRenderer* shadowRenderer;
	DirectionalVolumetricRenderer* directionalVolumetricRenderer;
	DirectionalLightRenderer* directionalLightRenderer;
	SkyboxRenderer* skyboxRenderer;
	PostProcessRenderer* postProcessRenderer;

	uint32_t rectangleVAO;

	static const char* shadowVertexPath;
	static const char* shadowFragmentPath;
	static const char* volumetricVertexPath;
	static const char* volumetricFragmentPath;
	static const char* bilateralBlurVertexPath;
	static const char* bilateralBlurFragmentPath;
	static const char* directionalLightVertexPath;
	static const char* directionalLightFragmentPath;
	static const char* skyboxVertexPath;
	static const char* skyboxFragmentPath;
	static const char* postProcessVertexPath;
	static const char* postProcessFragmentPath;	

};

#endif