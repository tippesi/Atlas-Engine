#include "DirectionalVolumetricRenderer.h"

DirectionalVolumetricRenderer::DirectionalVolumetricRenderer(const char *volumetricVertex, const char *volumetricFragment,
        const char *bilateralBlurVertex, const char *bilateralBlurFragmet) {

    framebuffer = new Framebuffer(0, 0);

    volumetricShader = new Shader();
    volumetricShader->AddComponent(VERTEX_SHADER, volumetricVertex);
    volumetricShader->AddComponent(FRAGMENT_SHADER, volumetricFragment);

    volumetricShader->Compile();

    GetVolumetricUniforms(false);

    bilateralBlurShader = new Shader();

    GetBilateralBlurUniforms(false);

}

void DirectionalVolumetricRenderer::Render(Window *window, RenderTarget *target, Camera *camera, Scene *scene, bool masterRenderer) {

    framebuffer->Bind();

    volumetricShader->Bind();

    inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);
    target->geometryFramebuffer->GetComponent(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE0);

    for (Light* light : scene->lights) {

        if (light->type != DIRECTIONAL_LIGHT || light->shadow == nullptr || light->volumetric == nullptr) {
            continue;
        }

        framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, light->volumetric->map);

        glViewport(0, 0, light->volumetric->map->width, light->volumetric->map->height);

        vec3 direction = normalize(vec3(camera->viewMatrix * vec4(light->direction, 0.0f)));

        lightDirection->SetValue(direction);
        shadowCascadeCount->SetValue(light->shadow->componentCount);
        sampleCount->SetValue(light->volumetric->sampleCount);

        light->shadow->maps->Bind(GL_TEXTURE1);
#ifdef ENGINE_OGL
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
#endif

        for (int32_t i = 0; i < light->shadow->componentCount; i++) {
            ShadowComponent* cascade = &light->shadow->components[i];
            cascades[i].distance->SetValue(cascade->farDistance);
            cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->inverseViewMatrix);
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    }

}

void DirectionalVolumetricRenderer::GetVolumetricUniforms(bool deleteUniforms) {

    if (deleteUniforms) {
        delete depthTexture;
        delete shadowTexture;
        delete lightDirection;
        delete inverseProjectionMatrix;
        delete sampleCount;
        delete shadowCascadeCount;
        for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++) {
            delete cascades[i].distance;
            delete cascades[i].lightSpace;
        }
    }

    depthTexture = volumetricShader->GetUniform("depthTexture");
    shadowTexture = volumetricShader->GetUniform("cascadeMaps");
    lightDirection = volumetricShader->GetUniform("light.direction");
    inverseProjectionMatrix = volumetricShader->GetUniform("ipMatrix");
    sampleCount = volumetricShader->GetUniform("sampleCount");
    shadowCascadeCount = volumetricShader->GetUniform("light.shadow.cascadeCount");

    for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++) {
        cascades[i].distance = volumetricShader->GetUniform(string("light.shadow.cascades[" + to_string(i) + "].distance").c_str());
        cascades[i].lightSpace = volumetricShader->GetUniform(string("light.shadow.cascades[" + to_string(i) + "].cascadeSpace").c_str());
    }

    depthTexture->SetValue(0);
    shadowTexture->SetValue(1);

}

void DirectionalVolumetricRenderer::GetBilateralBlurUniforms(bool deleteUniforms) {

    if (deleteUniforms) {

    }

}