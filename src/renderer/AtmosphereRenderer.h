#ifndef ATMOSPHERERENDERER_H
#define ATMOSPHERERENDERER_H

#include "../System.h"
#include "IRenderer.h"

class AtmosphereRenderer : public IRenderer {

public:
	AtmosphereRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

	static string vertexPath;
	static string fragmentPath;

private:
	void GetUniforms();

	VertexArray* vertexArray;

	Shader* shader;

	Uniform* viewMatrix;
	Uniform* projectionMatrix;
	Uniform* cameraLocation;
	Uniform* sunDirection;

};

#endif