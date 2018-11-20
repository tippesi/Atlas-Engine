#include "BlurRenderer.h"

BlurRenderer::BlurRenderer(const char *vertexSource, const char *fragmentSource, int32_t channelCount, bool bilateral,
        float* kernelOffsets, float* kernelWeights, int32_t kernelSize, bool bilateral) : kernelSize(kernelSize), bilateralBlur(bilateral) {

    shader = new Shader();

    shader->AddComponent(VERTEX_SHADER, vertexSource);
    shader->AddComponent(FRAGMENT_SHADER, fragmentSource);

    if (bilateral) {
        shader->AddMacro("BILATERAL");
    }

    if (channelCount == 1) {
        shader->AddMacro("BLUR_R");
    }
    else if (channelCount == 2) {
        shader->AddMacro("BLUR_RG");
    }
    else if (channelCount == 3) {
        shader->AddMacro("BLUR_RGB");
    }

    shader->Compile();

    GetUniforms();

    framebuffer = new Framebuffer(0, 0);

}

void BlurRenderer::Render(Window *window, RenderTarget *target, Camera *camera, Scene *scene, bool masterRenderer) {



}

void BlurRenderer::Render(Camera *camera, Scene *scene, Texture *texture, Texture *swapTexture, Texture* depthTexture) {

    framebuffer->Bind();



}