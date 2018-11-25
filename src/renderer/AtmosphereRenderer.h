#ifndef ATMOSPHERERENDERER_H
#define ATMOSPHERERENDERER_H

#include "../System.h"
#include "IRenderer.h"

class AtmosphereRenderer : public IRenderer {

public:
	AtmosphereRenderer(string vertexSource, string fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

private:
	void GetUniforms();

	VertexArray* vertexArray;

	Shader* shader;

	Uniform* modelViewProjectionMatrix;

};

#endif