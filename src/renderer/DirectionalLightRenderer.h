#ifndef AE_DIRECTIONALLIGHTRENDERER_H
#define AE_DIRECTIONALLIGHTRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class DirectionalLightRenderer : public IRenderer {

public:
	DirectionalLightRenderer();

	virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static std::string vertexPath;
	static std::string fragmentPath;

private:
	void GetUniforms();

	Shader shader;

	Uniform* diffuseTexture;
	Uniform* normalTexture;
	Uniform* materialTexture;
	Uniform* depthTexture;
	Uniform* aoTexture;
	Uniform* volumetricTexture;
	Uniform* shadowTexture;

	Uniform* inverseViewMatrix;
	Uniform* inverseProjectionMatrix;

	Uniform* lightDirection;
	Uniform* lightColor;
	Uniform* lightAmbient;

	Uniform* scatteringFactor;

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