#ifndef DIRECTIONALVOLUMETRICRENDERER_H
#define DIRECTIONALVOLUMETRICRENDERER_H

#include "../System.h"
#include "../Kernel.h"
#include "IRenderer.h"

class DirectionalVolumetricRenderer : public IRenderer{

public:
    DirectionalVolumetricRenderer();

    virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

	static std::string volumetricVertexPath;
	static std::string volumetricFragmentPath;
	static std::string bilateralBlurVertexPath;
	static std::string bilateralBlurFragmentPath;

private:
    void GetVolumetricUniforms();
    void GetBilateralBlurUniforms();

    Framebuffer* framebuffer;

	Kernel blurKernel;

    Shader volumetricShader;
    Shader bilateralBlurShader;

    // Volumetric shader uniforms
    Uniform* depthTexture;
    Uniform* shadowTexture;
    Uniform* lightDirection;
    Uniform* inverseProjectionMatrix;
    Uniform* sampleCount;
	Uniform* scattering;
	Uniform* framebufferResolution;

    Uniform* shadowCascadeCount;

    struct ShadowCascadeUniform {
        Uniform* distance;
        Uniform* lightSpace;
    }cascades[MAX_SHADOW_CASCADE_COUNT];

    // Bilateral blur shader uniforms
    Uniform* diffuseTexture;
	Uniform* bilateralDepthTexture;
    Uniform* blurDirection;
    Uniform* offsets;
    Uniform* weights;
    Uniform* kernelSize;

};

#endif