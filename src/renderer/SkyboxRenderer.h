#ifndef AE_SKYBOXRENDERER_H
#define AE_SKYBOXRENDERER_H

#include "../System.h"
#include "IRenderer.h"
#include "buffer/VertexArray.h"

class SkyboxRenderer : public IRenderer {

public:
	SkyboxRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static std::string vertexPath;
	static std::string fragmentPath;

private:
	void GetUniforms();

	VertexArray vertexArray;

	Shader shader;

	Uniform* skyCubemap;
	Uniform* modelViewProjectionMatrix;

};

#endif