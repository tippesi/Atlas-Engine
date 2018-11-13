#include "DirectionalVolumetricRenderer.h"

DirectionalVolumetricRenderer::DirectionalVolumetricRenderer(const char *volumetricVertex, const char *volumetricFragment,
        const char *bilateralBlurVertex, const char *bilateralBlurFragmet) {

    framebuffer = new Framebuffer(0, 0);

    volumetricShader = new Shader();
    volumetricShader->AddComponent(VERTEX_SHADER, volumetricVertex);
    volumetricShader->AddComponent(FRAGMENT_SHADER, volumetricFragment);

    volumetricShader->Compile();

    GetVolumetricUniforms();

    bilateralBlurShader = new Shader();
    bilateralBlurShader->AddComponent(VERTEX_SHADER, bilateralBlurVertex);
    bilateralBlurShader->AddComponent(FRAGMENT_SHADER, bilateralBlurFragmet);

    bilateralBlurShader->AddMacro("TEXTURE");
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
    target->geometryFramebuffer->GetComponent(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE0);

    for (Light* light : scene->lights) {

        if (light->type != DIRECTIONAL_LIGHT || light->shadow == nullptr || light->volumetric == nullptr) {
            continue;
        }

        glViewport(0, 0, light->volumetric->map->width, light->volumetric->map->height);

        framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, light->volumetric->map);

        vec3 direction = normalize(vec3(camera->viewMatrix * vec4(light->direction, 0.0f)));

        lightDirection->SetValue(direction);
        shadowCascadeCount->SetValue(light->shadow->componentCount);
        sampleCount->SetValue(light->volumetric->sampleCount);
		framebufferResolution->SetValue(vec2(light->volumetric->map->width, light->volumetric->map->height));

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

    bilateralBlurShader->Bind();

    diffuseTexture->SetValue(0);

    float offsetArray[] = {0, 1, 2, 3, 4};
    float weightArray[] = {1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f, 1/9.0f};

    offsets->SetValue(offsetArray, 5);
    weights->SetValue(weightArray, 5);
    kernelSize->SetValue(5);

    for (Light* light : scene->lights) {

        if (light->type != DIRECTIONAL_LIGHT || light->shadow == nullptr || light->volumetric == nullptr) {
            continue;
        }

        glViewport(0, 0, light->volumetric->map->width, light->volumetric->map->height);

        framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, light->volumetric->blurMap);

        light->volumetric->map->Bind(GL_TEXTURE0);

        blurDirection->SetValue(vec2(1.0f / (float)light->volumetric->map->width, 0.0f));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, light->volumetric->map);

        light->volumetric->blurMap->Bind(GL_TEXTURE0);

        blurDirection->SetValue(vec2(0.0f, 1.0f / (float)light->volumetric->map->height));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    }

}

void DirectionalVolumetricRenderer::GetVolumetricUniforms() {

    depthTexture = volumetricShader->GetUniform("depthTexture");
    shadowTexture = volumetricShader->GetUniform("cascadeMaps");
    lightDirection = volumetricShader->GetUniform("light.direction");
    inverseProjectionMatrix = volumetricShader->GetUniform("ipMatrix");
    sampleCount = volumetricShader->GetUniform("sampleCount");
    shadowCascadeCount = volumetricShader->GetUniform("light.shadow.cascadeCount");
	framebufferResolution = volumetricShader->GetUniform("framebufferResolution");

    for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++) {
        cascades[i].distance = volumetricShader->GetUniform(string("light.shadow.cascades[" + to_string(i) + "].distance").c_str());
        cascades[i].lightSpace = volumetricShader->GetUniform(string("light.shadow.cascades[" + to_string(i) + "].cascadeSpace").c_str());
    }

}

void DirectionalVolumetricRenderer::GetBilateralBlurUniforms() {

    diffuseTexture = bilateralBlurShader->GetUniform("diffuseTexture");
    blurDirection = bilateralBlurShader->GetUniform("blurDirection");
    offsets = bilateralBlurShader->GetUniform("offset");
    weights = bilateralBlurShader->GetUniform("weight");
    kernelSize = bilateralBlurShader->GetUniform("kernelSize");

}