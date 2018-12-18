#include "BlurRenderer.h"

BlurRenderer::BlurRenderer(string vertexSource, string fragmentSource, int32_t channelCount,
        float* kernelOffsets, float* kernelWeights, int32_t kernelSize, bool bilateral) : bilateralBlur(bilateral) {

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

    ShaderSource* fragmentShaderSource = shader->GetComponent(FRAGMENT_SHADER);

    ShaderConstant* kernelOffsetsConstant = fragmentShaderSource->GetConstant("offset");
    ShaderConstant* kernelWeightsConstant = fragmentShaderSource->GetConstant("weight");
    ShaderConstant* kernelSizeConstant = fragmentShaderSource->GetConstant("kernelSize");

    kernelOffsetsConstant->SetValue(kernelOffsets, kernelSize);
    kernelWeightsConstant->SetValue(kernelWeights, kernelSize);
    kernelSizeConstant->SetValue(kernelSize);

    shader->Compile();

    GetUniforms();

    framebuffer = new Framebuffer(0, 0);

}

void BlurRenderer::Render(Window *window, RenderTarget *target, Camera *camera, Scene *scene, bool masterRenderer) {

    return;

}

void BlurRenderer::Render(Texture2D *texture, Texture2D *swapTexture, Texture2D* depthTexture) {

    framebuffer->Bind();

    diffuseTexture->SetValue(0);
    this->depthTexture->SetValue(1);

    if (bilateralBlur) {
        depthTexture->Bind(GL_TEXTURE1);
    }

    glViewport(0, 0, texture->width, texture->height);

    framebuffer->AddComponentTexture(GL_COLOR_ATTACHMENT0, swapTexture);

    texture->Bind(GL_TEXTURE0);

    blurDirection->SetValue(vec2(1.0f / (float)texture->width, 0.0f));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    framebuffer->AddComponentTexture(GL_COLOR_ATTACHMENT0, texture);

    swapTexture->Bind(GL_TEXTURE0);

    blurDirection->SetValue(vec2(0.0f, 1.0f / (float)texture->height));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

}

void BlurRenderer::GetUniforms() {

    diffuseTexture = shader->GetUniform("diffuseTexture");
    depthTexture = shader->GetUniform("depthTexture");
    blurDirection = shader->GetUniform("blurDirection");

}