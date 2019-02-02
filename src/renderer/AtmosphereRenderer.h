#ifndef AE_ATMOSPHERERENDERER_H
#define AE_ATMOSPHERERENDERER_H

#include "../System.h"
#include "IRenderer.h"

class AtmosphereRenderer : public IRenderer {

public:
	AtmosphereRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static std::string vertexPath;
	static std::string fragmentPath;

private:
	void GetUniforms();

	VertexArray vertexArray;

	Shader shader;

	Uniform* viewMatrix;
	Uniform* projectionMatrix;
	Uniform* cameraLocation;
	Uniform* sunDirection;

};

#endif