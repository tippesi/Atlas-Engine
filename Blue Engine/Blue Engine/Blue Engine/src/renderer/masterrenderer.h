#ifndef MASTERRENDERER_H
#define MASTERRENDERER_H

#include "../system.h"

#include "geometryrenderer.h"
#include "postprocessrenderer.h"

class MasterRenderer {

public:
	MasterRenderer(const char* shaderDirectory);

	void RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	void RenderTexture(Texture* texture);

	void RenderRectangle(vec3 color);

	static uint32_t GenerateRectangleVAO();

private:
	GeometryRenderer * geometryRenderer;
	PostProcessRenderer * postProcessRenderer;

	uint32_t rectangleVAO;

	static const char* postProcessVertexPath;
	static const char* postProcessFragmentPath;	

};

#endif