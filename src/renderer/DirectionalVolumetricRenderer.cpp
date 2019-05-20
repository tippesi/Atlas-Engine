#include "DirectionalVolumetricRenderer.h"
#include "../lighting/DirectionalLight.h"

namespace Atlas {

    namespace Renderer {

        std::string DirectionalVolumetricRenderer::volumetricVertexPath = "volumetric.vsh";
        std::string DirectionalVolumetricRenderer::volumetricFragmentPath = "volumetric.fsh";
        std::string DirectionalVolumetricRenderer::bilateralBlurVertexPath = "bilateralBlur.vsh";
        std::string DirectionalVolumetricRenderer::bilateralBlurFragmentPath = "bilateralBlur.fsh";

        DirectionalVolumetricRenderer::DirectionalVolumetricRenderer() {

            blurKernel.CalculateBoxFilter(21);

            volumetricShader.AddStage(AE_VERTEX_STAGE, volumetricVertexPath);
            volumetricShader.AddStage(AE_FRAGMENT_STAGE, volumetricFragmentPath);

            volumetricShader.Compile();

            GetVolumetricUniforms();

            bilateralBlurShader.AddStage(AE_VERTEX_STAGE, bilateralBlurVertexPath);
            bilateralBlurShader.AddStage(AE_FRAGMENT_STAGE, bilateralBlurFragmentPath);

            bilateralBlurShader.AddMacro("BILATERAL");
            bilateralBlurShader.AddMacro("BLUR_R");

            bilateralBlurShader.Compile();

            GetBilateralBlurUniforms();

        }

        void DirectionalVolumetricRenderer::Render(Viewport *viewport, RenderTarget *target,
                Camera *camera, Scene::Scene *scene) {

            framebuffer.Bind();

            volumetricShader.Bind();

            inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);
            target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE0);

			auto lights = scene->GetLights();

            for (auto& light : lights) {

                if (light->type != AE_DIRECTIONAL_LIGHT || !light->GetShadow() || !light->GetVolumetric()) {
                    continue;
                }

                auto directionalLight = (Lighting::DirectionalLight*)light;

                glViewport(0, 0, directionalLight->GetVolumetric()->map->width, directionalLight->GetVolumetric()->map->height);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, directionalLight->GetVolumetric()->map);

                vec3 direction = normalize(vec3(camera->viewMatrix * vec4(directionalLight->direction, 0.0f)));

                lightDirection->SetValue(direction);
                shadowCascadeCount->SetValue(directionalLight->GetShadow()->componentCount);
                sampleCount->SetValue(directionalLight->GetVolumetric()->sampleCount);
                scattering->SetValue(glm::clamp(directionalLight->GetVolumetric()->scattering, -1.0f, 1.0f));
                framebufferResolution->SetValue(vec2(directionalLight->GetVolumetric()->map->width,
                                                     directionalLight->GetVolumetric()->map->height));

                light->GetShadow()->maps.Bind(GL_TEXTURE1);

                for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++) {
                    if (i < light->GetShadow()->componentCount) {
						auto cascade = &directionalLight->GetShadow()->components[i];
                        cascades[i].distance->SetValue(cascade->farDistance);
                        cascades[i].lightSpace->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->inverseViewMatrix);
                    }
                    else {
                        cascades[i].distance->SetValue(camera->farPlane);
                        cascades[i].lightSpace->SetValue(mat4(1.0f));
                    }
                }

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            }

            bilateralBlurShader.Bind();

            target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);

            std::vector<float>* kernelWeights;
            std::vector<float>* kernelOffsets;

            blurKernel.GetLinearized(kernelWeights, kernelOffsets);

            weights->SetValue(kernelWeights->data(), (int32_t)kernelWeights->size());
            offsets->SetValue(kernelOffsets->data(), (int32_t)kernelOffsets->size());

            kernelSize->SetValue((int32_t)kernelWeights->size());

            for (auto& light : lights) {

                if (light->type != AE_DIRECTIONAL_LIGHT || light->GetShadow() == nullptr || light->GetVolumetric() == nullptr) {
                    continue;
                }

                auto directionalLight = (Lighting::DirectionalLight*)light;

                glViewport(0, 0, directionalLight->GetVolumetric()->map->width, directionalLight->GetVolumetric()->map->height);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, directionalLight->GetVolumetric()->blurMap);

                directionalLight->GetVolumetric()->map->Bind(GL_TEXTURE0);

                blurDirection->SetValue(vec2(1.0f / (float)directionalLight->GetVolumetric()->map->width, 0.0f));

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, directionalLight->GetVolumetric()->map);

                directionalLight->GetVolumetric()->blurMap->Bind(GL_TEXTURE0);

                blurDirection->SetValue(vec2(0.0f, 1.0f / (float)directionalLight->GetVolumetric()->map->height));

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            }

        }

        void DirectionalVolumetricRenderer::GetVolumetricUniforms() {

            lightDirection = volumetricShader.GetUniform("light.direction");
            inverseProjectionMatrix = volumetricShader.GetUniform("ipMatrix");
            sampleCount = volumetricShader.GetUniform("sampleCount");
            scattering = volumetricShader.GetUniform("scattering");
            shadowCascadeCount = volumetricShader.GetUniform("light.shadow.cascadeCount");
            framebufferResolution = volumetricShader.GetUniform("framebufferResolution");

            for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++) {
                cascades[i].distance = volumetricShader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].distance");
                cascades[i].lightSpace = volumetricShader.GetUniform("light.shadow.cascades[" + std::to_string(i) + "].cascadeSpace");
            }

        }

        void DirectionalVolumetricRenderer::GetBilateralBlurUniforms() {

            blurDirection = bilateralBlurShader.GetUniform("blurDirection");
            offsets = bilateralBlurShader.GetUniform("offset");
            weights = bilateralBlurShader.GetUniform("weight");
            kernelSize = bilateralBlurShader.GetUniform("kernelSize");

        }

    }

}