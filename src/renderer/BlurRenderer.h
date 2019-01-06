#ifndef BLURRENDERER_H
#define BLURRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class BlurRenderer : IRenderer {

public:
    BlurRenderer(string vertexSource, string fragmentSource, int32_t channelCount, float* kernelOffsets,
            float* kernelWeights, int32_t kernelSize, bool bilateral = false);

    virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene);

    void Render(Texture2D* texture, Texture2D* swapTexture, Texture2D* depthTexture);

private:
    void GetUniforms();

    float* kernelOffsets;
    float* kernelWeights;
    int32_t kernelSize;

    bool bilateralBlur;

    Framebuffer* framebuffer;

    Shader shader;

    Uniform* diffuseTexture;
    Uniform* depthTexture;
    Uniform* blurDirection;


};


#endif
