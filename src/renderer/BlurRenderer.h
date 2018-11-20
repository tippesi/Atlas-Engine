#ifndef BLURRENDERER_H
#define BLURRENDERER_H

#include "../System.h"
#include "IRenderer.h"

class BlurRenderer : IRenderer {

public:
    BlurRenderer(const char* vertexSource, const char* fragmentSource, int32_t channelCount, float* kernelOffsets,
            float* kernelWeights, int32_t kernelSize, bool bilateral = false);

    virtual void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer = false)

    void Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, Texture* texture, Texture* swapTexture);

private:
    void GetUniforms();

    float* kernelOffsets;
    float* kernelWeights;
    int32_t kernelSize;

    bool bilateralBlur;

    Framebuffer* framebuffer;

    Shader* shader;

    Uniform* diffuseTexture;
    Uniform* bilateralDepthTexture;
    Uniform* blurDirection;
    Uniform* offsets;
    Uniform* weights;
    Uniform* kernelSize;


};


#endif
