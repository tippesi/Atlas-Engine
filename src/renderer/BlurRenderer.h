#ifndef BLURRENDERER_H
#define BLURRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class BlurRenderer : IRenderer {

public:
    BlurRenderer(string vertexSource, string fragmentSource, int32_t channelCount, float* kernelOffsets,
            float* kernelWeights, int32_t kernelSize, bool bilateral = false);

    virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false);

    void Render(Texture* texture, Texture* swapTexture, Texture* depthTexture);

private:
    void GetUniforms();

    float* kernelOffsets;
    float* kernelWeights;
    int32_t kernelSize;

    bool bilateralBlur;

    Framebuffer* framebuffer;

    Shader* shader;

    Uniform* diffuseTexture;
    Uniform* depthTexture;
    Uniform* blurDirection;


};


#endif
