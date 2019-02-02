#ifndef AE_POINTLIGHTRENDERER_H
#define AE_POINTLIGHTRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class PointLightRenderer : public IRenderer {

public:
	PointLightRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static std::string vertexPath;
	static std::string fragmentPath;

private:
	void GetUniforms();

	VertexArray vertexArray;

	Shader shader;

	Uniform* diffuseTexture;
	Uniform* normalTexture;
	Uniform* materialTexture;
	Uniform* depthTexture;
	Uniform* shadowCubemap;

	Uniform* viewMatrix;
	Uniform* projectionMatrix;
	Uniform* inverseProjectionMatrix;
	Uniform* lightViewMatrix;
	Uniform* lightProjectionMatrix;
	Uniform* viewSpaceLightLocation;
	Uniform* lightLocation;
	Uniform* lightColor;
	Uniform* lightAmbient;
	Uniform* lightRadius;

};

#endif