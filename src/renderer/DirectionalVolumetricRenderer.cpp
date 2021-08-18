#include "DirectionalVolumetricRenderer.h"
#include "../lighting/DirectionalLight.h"

namespace Atlas {

    namespace Renderer {

        DirectionalVolumetricRenderer::DirectionalVolumetricRenderer() {

            blurFilter.CalculateBoxFilter(21);

            volumetricShader.AddStage(AE_VERTEX_STAGE, "volumetric.vsh");
            volumetricShader.AddStage(AE_FRAGMENT_STAGE, "volumetric.fsh");

            volumetricShader.Compile();

            bilateralBlurShader.AddStage(AE_VERTEX_STAGE, "bilateralBlur.vsh");
            bilateralBlurShader.AddStage(AE_FRAGMENT_STAGE, "bilateralBlur.fsh");

            bilateralBlurShader.AddMacro("BILATERAL");
            bilateralBlurShader.AddMacro("BLUR_R");

            bilateralBlurShader.Compile();

        }

        void DirectionalVolumetricRenderer::Render(Viewport *viewport, RenderTarget *target,
                Camera *camera, Scene::Scene *scene) {

            framebuffer.Bind();

            volumetricShader.Bind();

            volumetricShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
            target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE0);

			auto lights = scene->GetLights();

            for (auto& light : lights) {

                auto volumetric = light->GetVolumetric();
                auto shadow = light->GetShadow();

                if (light->type != AE_DIRECTIONAL_LIGHT || !volumetric || !shadow) continue;

                auto directionalLight = (Lighting::DirectionalLight*)light;

                glViewport(0, 0, volumetric->map.width, volumetric->map.height);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &volumetric->map);

                vec3 direction = normalize(vec3(camera->viewMatrix * vec4(directionalLight->direction, 0.0f)));

                volumetricShader.GetUniform("light.direction")->SetValue(direction);
                volumetricShader.GetUniform("light.shadow.cascadeCount")->SetValue(shadow->componentCount);
                volumetricShader.GetUniform("sampleCount")->SetValue(volumetric->sampleCount);
                volumetricShader.GetUniform("framebufferResolution")->SetValue(vec2(volumetric->map.width, volumetric->map.height));
                volumetricShader.GetUniform("intensity")->SetValue(volumetric->intensity);

                light->GetShadow()->maps.Bind(GL_TEXTURE1);

                for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
                    auto cascadeString = "light.shadow.cascades[" + std::to_string(i) + "]";
                    if (i < shadow->componentCount) {
						auto cascade = &shadow->components[i];
                        volumetricShader.GetUniform(cascadeString + ".distance")->SetValue(cascade->farDistance);
                        volumetricShader.GetUniform(cascadeString + ".cascadeSpace")->SetValue(cascade->projectionMatrix * cascade->viewMatrix * camera->invViewMatrix);
                    }
                    else {
                        volumetricShader.GetUniform(cascadeString + ".distance")->SetValue(camera->farPlane);
                    }
                }

                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            }

            bilateralBlurShader.Bind();

            target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE1);

            std::vector<float> kernelWeights;
            std::vector<float> kernelOffsets;

            blurFilter.GetLinearized(&kernelWeights, &kernelOffsets);

            bilateralBlurShader.GetUniform("weight")->SetValue(kernelWeights.data(), (int32_t)kernelWeights.size());
            bilateralBlurShader.GetUniform("offset")->SetValue(kernelOffsets.data(), (int32_t)kernelOffsets.size());

            bilateralBlurShader.GetUniform("kernelSize")->SetValue((int32_t)kernelWeights.size());

            for (auto& light : lights) {

                auto volumetric = light->GetVolumetric();
                auto shadow = light->GetShadow();

                if (light->type != AE_DIRECTIONAL_LIGHT || !volumetric || !shadow) continue;

                glViewport(0, 0, volumetric->map.width, volumetric->map.height);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &volumetric->blurMap);

                volumetric->map.Bind(GL_TEXTURE0);
                bilateralBlurShader.GetUniform("blurDirection")->SetValue(vec2(1.0f / (float)volumetric->map.width, 0.0f));
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &volumetric->map);

                volumetric->blurMap.Bind(GL_TEXTURE0);
                bilateralBlurShader.GetUniform("blurDirection")->SetValue(vec2(0.0f, 1.0f / (float)volumetric->map.height));
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            }

        }

    }

}