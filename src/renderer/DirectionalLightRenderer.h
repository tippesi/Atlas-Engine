#ifndef DIRECTIONALLIGHTRENDERER_H
#define DIRECTIONALLIGHTRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class DirectionalLightRenderer : public IRenderer {

public:
	DirectionalLightRenderer(string vertexSource, string fragmentSource);

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

private:
	void GetUniforms();

	uint32_t rectangleVAO;

	Shader* shader;

	Uniform* diffuseTexture;
	Uniform* normalTexture;
	Uniform* materialTexture;
	Uniform* depthTexture;
	Uniform* aoTexture;
	Uniform* volumetricTexture;
	Uniform* shadowTexture;

	Uniform* lightDirection;
	Uniform* lightColor;
	Uniform* lightAmbient;

	Uniform* inverseViewMatrix;
	Uniform* inverseProjectionMatrix;

	Uniform* shadowDistance;
	Uniform* shadowBias;
	Uniform* shadowSampleCount;
	Uniform* shadowSampleRange;
	Uniform* shadowSampleRandomness;
	Uniform* shadowCascadeCount;
	Uniform* shadowResolution;

	struct ShadowCascadeUniform {
		Uniform* distance;
		Uniform* lightSpace;
	}cascades[MAX_SHADOW_CASCADE_COUNT];

};

#endif