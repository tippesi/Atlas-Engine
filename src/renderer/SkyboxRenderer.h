#ifndef SKYBOXRENDERER_H
#define SKYBOXRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "buffer/VertexArray.h"

class SkyboxRenderer : public IRenderer {

public:
	SkyboxRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static string vertexPath;
	static string fragmentPath;

private:
	void GetUniforms();

	VertexArray vertexArray;

	Shader shader;

	Uniform* skyCubemap;
	Uniform* modelViewProjectionMatrix;

};

#endif