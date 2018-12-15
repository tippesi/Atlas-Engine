#include "DirectionalVolumetricRenderer.h"
#include "../lighting/DirectionalLight.h"

string DirectionalVolumetricRenderer::volumetricVertexPath = "volumetric.vsh";
string DirectionalVolumetricRenderer::volumetricFragmentPath = "volumetric.fsh";
string DirectionalVolumetricRenderer::bilateralBlurVertexPath = "bilateralBlur.vsh";
string DirectionalVolumetricRenderer::bilateralBlurFragmentPath = "bilateralBlur.fsh";

DirectionalVolumetricRenderer::DirectionalVolumetricRenderer() {

    framebuffer = new Framebuffer(0, 0);

	blurKernel = new Kernel();
	blurKernel->CalculateBoxFilter(21);

    volumetricShader = new Shader();
    volumetricShader->AddComponent(VERTEX_SHADER, volumetricVertexPath);
    volumetricShader->AddComponent(FRAGMENT_SHADER, volumetricFragmentPath);

    volumetricShader->Compile();

    GetVolumetricUniforms();

    bilateralBlurShader = new Shader();
    bilateralBlurShader->AddComponent(VERTEX_SHADER, bilateralBlurVertexPath);
    bilateralBlurShader->AddComponent(FRAGMENT_SHADER, bilateralBlurFragmentPath);

    bilateralBlurShader->AddMacro("BILATERAL");
    bilateralBlurShader->AddMacro("BLUR_R");

    bilateralBlurShader->Compile();

    GetBilateralBlurUniforms();

}

void DirectionalVolumetricRenderer::Render(Window *window, RenderTarget *target, Camera *camera, Scene *scene, bool masterRenderer) {

    framebuffer->Bind();

    volumetricShader->Bind();

	depthTexture->SetValue(0);
	shadowTexture->SetValue(1);

    inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);
    target->geometryFramebuffer->GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE0);

    for (ILight* light : scene->lights) {

        if (light->type != DIRECTIONAL_LIGHT || light->GetShadow() == nullptr || light->GetVolumetric() == nullptr) {
            continue;
        }

        DirectionalLight* directionalLight = (DirectionalLight*)light;

        glViewport(0, 0, directionalLight->GetVolumetric()->map->width, directionalLight->GetVolumetric()->map->height);

        framebuffer->AddComponentTexture(GL_COLOR_ATTACHMENT0, directionalLight->GetVolumetric()->map);

        vec3 direction = normalize(vec3(camera->viewMatrix * vec4(directionalLight->direction, 0.0f)));

        lightDirection->SetValue(direction);
        shadowCascadeCount->SetValue(directionalLight->GetShadow()->componentCount);
        sampleCount->SetValue(directionalLight->GetVolumetric()->sampleCount);
		scattering->SetValue(glm::clamp(directionalLight->GetVolumetric()->scattering, -1.0f, 1.0f));
		framebufferResolution->SetValue(vec2(directionalLight->GetVolumetric()->map->width,
		        directionalLight->GetVolumetric()->map->height));

        light->GetShadow()->maps->Bind(GL_TEXTURE1);

        for (int32_t i = 0; i < light->GetShadow()->componentCount; i++) {
            ShadowComponent* cascade = &directionalLight->GetShadow()->components[i];
            cascades[i].distance->SetValue(cascade->farDistance);
            cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->inverseViewMatrix);
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    }
	
    bilateralBlurShader->Bind();

    diffuseTexture->SetValue(0);
	bilateralDepthTexture->SetValue(1);

	target->geometryFramebuffer->GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);

	vector<float>* kernelWeights;
	vector<float>* kernelOffsets;

	blurKernel->GetLinearized(kernelWeights, kernelOffsets);

	weights->SetValue(kernelWeights->data(), (int32_t)kernelWeights->size());
    offsets->SetValue(kernelOffsets->data(), (int32_t)kernelOffsets->size());

    kernelSize->SetValue((int32_t)kernelWeights->size());

    for (ILight* light : scene->lights) {

        if (light->type != DIRECTIONAL_LIGHT || light->GetShadow() == nullptr || light->GetVolumetric() == nullptr) {
            continue;
        }

        DirectionalLight* directionalLight = (DirectionalLight*)light;

        glViewport(0, 0, directionalLight->GetVolumetric()->map->width, directionalLight->GetVolumetric()->map->height);

        framebuffer->AddComponentTexture(GL_COLOR_ATTACHMENT0, directionalLight->GetVolumetric()->blurMap);

        directionalLight->GetVolumetric()->map->Bind(GL_TEXTURE0);

        blurDirection->SetValue(vec2(1.0f / (float)directionalLight->GetVolumetric()->map->width, 0.0f));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        framebuffer->AddComponentTexture(GL_COLOR_ATTACHMENT0, directionalLight->GetVolumetric()->map);

        directionalLight->GetVolumetric()->blurMap->Bind(GL_TEXTURE0);

        blurDirection->SetValue(vec2(0.0f, 1.0f / (float)directionalLight->GetVolumetric()->map->height));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    }

}

void DirectionalVolumetricRenderer::GetVolumetricUniforms() {

    depthTexture = volumetricShader->GetUniform("depthTexture");
    shadowTexture = volumetricShader->GetUniform("cascadeMaps");
    lightDirection = volumetricShader->GetUniform("light.direction");
    inverseProjectionMatrix = volumetricShader->GetUniform("ipMatrix");
    sampleCount = volumetricShader->GetUniform("sampleCount");
	scattering = volumetricShader->GetUniform("scattering");
    shadowCascadeCount = volumetricShader->GetUniform("light.shadow.cascadeCount");
	framebufferResolution = volumetricShader->GetUniform("framebufferResolution");

    for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++) {
        cascades[i].distance = volumetricShader->GetUniform("light.shadow.cascades[" + to_string(i) + "].distance");
        cascades[i].lightSpace = volumetricShader->GetUniform("light.shadow.cascades[" + to_string(i) + "].cascadeSpace");
    }

}

void DirectionalVolumetricRenderer::GetBilateralBlurUniforms() {

    diffuseTexture = bilateralBlurShader->GetUniform("diffuseTexture");
	bilateralDepthTexture = bilateralBlurShader->GetUniform("depthTexture");
    blurDirection = bilateralBlurShader->GetUniform("blurDirection");
    offsets = bilateralBlurShader->GetUniform("offset");
    weights = bilateralBlurShader->GetUniform("weight");
    kernelSize = bilateralBlurShader->GetUniform("kernelSize");

}