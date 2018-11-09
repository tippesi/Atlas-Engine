#ifndef DIRECTIONALLIGHTRENDERER_H
#define DIRECTIONALLIGHTRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class DirectionalLightRenderer : public IRenderer {

public:
	DirectionalLightRenderer(const char* vertexSource, const char* fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

private:
	void GetUniforms(bool deleteUniforms);

	uint32_t rectangleVAO;

	Shader* shader;

	Uniform* diffuseTexture;
	Uniform* normalTexture;
	Uniform* materialTexture;
	Uniform* depthTexture;
	Uniform* aoTexture;
	Uniform* firstCascadeTexture;

	Uniform* lightDirection;
	Uniform* lightColor;
	Uniform* lightAmbient;

	Uniform* lightSpaceMatrix;
	Uniform* inverseViewMatrix;
	Uniform* inverseProjectionMatrix;

	Uniform* shadowDistance;
	Uniform* shadowBias;
	Uniform* shadowSampleCount;
	Uniform* shadowSampleRange;
	Uniform* shadowSampleRandomness;

};

#endif