#ifndef SKYBOXRENDERER_H
#define SKYBOXRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class SkyboxRenderer : public IRenderer {

public:
	SkyboxRenderer(string vertexSource, string fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

private:
	Shader* shader;

	Uniform* skyCubemap;
	Uniform* modelViewProjectionMatrix;

	uint32_t skyboxVAO;

	static const float skyboxVertices[];

};

#endif