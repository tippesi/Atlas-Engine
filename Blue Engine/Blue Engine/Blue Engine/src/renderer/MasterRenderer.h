#ifndef MASTERRENDERER_H
#define MASTERRENDERER_H

#include "../System.h"

#include "GeometryRenderer.h"
#include "ShadowRenderer.h"
#include "DirectionalLightRenderer.h"
#include "PostProcessRenderer.h"

class MasterRenderer {

public:
	MasterRenderer(const char* shaderDirectory);

	void RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	void RenderTexture(Texture* texture);

	void RenderRectangle(vec3 color);

	static uint32_t GenerateRectangleVAO();

private:
	GeometryRenderer* geometryRenderer;
	ShadowRenderer* shadowRenderer;
	DirectionalLightRenderer* directionalLightRenderer;
	PostProcessRenderer* postProcessRenderer;

	uint32_t rectangleVAO;

	static const char* shadowVertexPath;
	static const char* shadowFragmentPath;
	static const char* directionalLightVertexPath;
	static const char* directionalLightFragmentPath;
	static const char* postProcessVertexPath;
	static const char* postProcessFragmentPath;	

};

#endif