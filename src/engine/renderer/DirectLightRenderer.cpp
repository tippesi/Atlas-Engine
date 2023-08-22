#include "DirectLightRenderer.h"

namespace Atlas {

    namespace Renderer {

        void DirectLightRenderer::Init(Graphics::GraphicsDevice* device) {

            this->device = device;

            auto bufferDesc = Graphics::BufferDesc {
                .usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                .domain = Graphics::BufferDomain::Host,
                .size = sizeof(Uniforms)
            };
            uniformBuffer = device->CreateMultiBuffer(bufferDesc);

            pipelineConfig = PipelineConfig("deferred/direct.csh");

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_LINEAR,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

        }

        void DirectLightRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

            if (!scene->sky.sun) return;

            Graphics::Profiler::BeginQuery("Direct lighting");

            auto light = scene->sky.sun;
            auto sss = scene->sss;
            auto clouds = scene->sky.clouds;

            vec3 direction = normalize(vec3(camera->viewMatrix * vec4(light->direction, 0.0f)));

            Uniforms uniforms;

            auto& lightUniform = uniforms.light;
            lightUniform.location = vec4(0.0f);
            lightUniform.direction = vec4(direction, 0.0f);
            lightUniform.color = vec4(light->color, 0.0f);
            lightUniform.intensity = light->intensity;
            lightUniform.scatteringFactor = 1.0f;
            lightUniform.radius = 1.0f;

            if (light->GetShadow()) {
                auto shadow = light->GetShadow();
                auto& shadowUniform = lightUniform.shadow;
                shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                shadowUniform.bias = shadow->bias;
                shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                shadowUniform.cascadeCount = shadow->componentCount;
                shadowUniform.resolution = vec2(shadow->resolution);

                if (shadow->useCubemap) {
                    commandList->BindImage(shadow->cubemap.image, shadowSampler, 3, 1);
                }
                else {
                    commandList->BindImage(shadow->maps.image, shadowSampler, 3, 1);
                }

                auto componentCount = shadow->componentCount;
                for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
                    if (i < componentCount) {
                        auto cascade = &shadow->components[i];
                        auto frustum = Volume::Frustum(cascade->frustumMatrix);
                        auto corners = frustum.GetCorners();
                        auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                            abs(corners[1].y - corners[3].y)) / (float)light->GetShadow()->resolution;
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                        shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix *
                            cascade->viewMatrix * camera->invViewMatrix;
                        shadowUniform.cascades[i].texelSize = texelSize;
                    }
                    else {
                        auto cascade = &shadow->components[componentCount - 1];
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                    }
                }
            }

            pipelineConfig.ManageMacro("SCREEN_SPACE_SHADOWS", sss && sss->enable);
            pipelineConfig.ManageMacro("CLOUD_SHADOWS", clouds && clouds->enable && clouds->castShadow);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            commandList->BindBuffer(uniformBuffer, 3, 4);

            if (sss && sss->enable) {
                commandList->BindImage(target->sssTexture.image, target->sssTexture.sampler, 3, 2);
            }

            if (clouds && clouds->enable && clouds->castShadow) {
                clouds->shadowTexture.Bind(commandList, 3, 3);

                clouds->GetShadowMatrices(camera, glm::normalize(light->direction),
                    uniforms.cloudShadowViewMatrix, uniforms.cloudShadowProjectionMatrix);
            }

            uniformBuffer->SetData(&uniforms, 0, sizeof(uniforms));

            ivec2 res = ivec2(target->GetWidth(), target->GetHeight());
            int32_t groupSize = 8;
            ivec2 groupCount = res / groupSize;
            groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
            groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::Profiler::EndQuery();

        }

    }

}
