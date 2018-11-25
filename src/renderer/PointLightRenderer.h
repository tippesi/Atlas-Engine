#ifndef POINTLIGHTRENDERER_H
#define POINTLIGHTRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class PointLightRenderer : public IRenderer {

public:
	PointLightRenderer(string vertexSource, string fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

private:
	void GetUniforms();

	VertexArray* vertexArray;

	Shader* shader;

	Uniform* diffuseTexture;
	Uniform* normalTexture;
	Uniform* materialTexture;
	Uniform* depthTexture;

	Uniform* viewProjectionMatrix;
	Uniform* inverseProjectionMatrix;
	Uniform* viewSpaceLightLocation;
	Uniform* lightLocation;
	Uniform* lightColor;
	Uniform* lightAmbient;
	Uniform* lightRadius;

};

#endif