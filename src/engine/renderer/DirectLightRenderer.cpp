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
            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));
            cloudShadowUniformBuffer = Buffer::UniformBuffer(sizeof(CloudShadow));

            pipelineConfig = PipelineConfig("deferred/direct.csh");

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_LINEAR,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

        }

        void DirectLightRenderer::Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList) {

            auto mainLightEntity = GetMainLightEntity(scene);
            if (!mainLightEntity.IsValid()) return;

            Graphics::Profiler::BeginQuery("Direct lighting");

            auto& camera = scene->GetMainCamera();
            auto& light = mainLightEntity.GetComponent<LightComponent>();
            auto sss = scene->sss;
            auto clouds = scene->sky.clouds;

            vec3 direction = normalize(vec3(camera.viewMatrix *
                vec4(light.transformedProperties.directional.direction, 0.0f)));

            Uniforms uniforms;

            auto& lightUniform = uniforms.light;
            lightUniform.location = vec4(0.0f);
            lightUniform.direction = vec4(direction, 0.0f);
            lightUniform.color = vec4(Common::ColorConverter::ConvertSRGBToLinear(light.color), 0.0f);
            lightUniform.intensity = light.intensity;
            lightUniform.scatteringFactor = 1.0f;
            lightUniform.radius = 1.0f;

            if (light.shadow) {
                auto shadow = light.shadow;
                auto& shadowUniform = lightUniform.shadow;
                shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                shadowUniform.bias = shadow->bias;
                shadowUniform.edgeSoftness = shadow->edgeSoftness;
                shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                shadowUniform.cascadeCount = shadow->viewCount;
                shadowUniform.resolution = vec2(shadow->resolution);

                if (shadow->useCubemap) {
                    commandList->BindImage(shadow->cubemap.image, shadowSampler, 3, 1);
                }
                else {
                    commandList->BindImage(shadow->maps.image, shadowSampler, 3, 1);
                }

                auto componentCount = shadow->viewCount;
                for (int32_t i = 0; i < MAX_SHADOW_VIEW_COUNT + 1; i++) {
                    if (i < componentCount) {
                        auto cascade = &shadow->views[i];
                        auto frustum = Volume::Frustum(cascade->frustumMatrix);
                        auto corners = frustum.GetCorners();
                        auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                            abs(corners[1].y - corners[3].y)) / (float)shadow->resolution;
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                        shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix *
                            cascade->viewMatrix * camera.invViewMatrix;
                        shadowUniform.cascades[i].texelSize = texelSize;
                    }
                    else {
                        auto cascade = &shadow->views[componentCount - 1];
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                    }
                }
            }

            uniformBuffer.SetData(&uniforms, 0);

            pipelineConfig.ManageMacro("SHADOWS", light.shadow != nullptr);
            pipelineConfig.ManageMacro("SCREEN_SPACE_SHADOWS", sss && sss->enable);
            pipelineConfig.ManageMacro("CLOUD_SHADOWS", clouds && clouds->enable && clouds->castShadow);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
            commandList->BindPipeline(pipeline);

            commandList->BindImage(target->lightingTexture.image, 3, 0);
            uniformBuffer.Bind(commandList, 3, 4);

            if (sss && sss->enable) {
                commandList->BindImage(target->sssTexture.image, target->sssTexture.sampler, 3, 2);
            }

            CloudShadow cloudShadowUniform;
            if (clouds && clouds->enable && clouds->castShadow) {
                clouds->shadowTexture.Bind(commandList, 3, 3);

                clouds->GetShadowMatrices(camera, glm::normalize(light.transformedProperties.directional.direction),
                    cloudShadowUniform.vMatrix, cloudShadowUniform.pMatrix);

                cloudShadowUniform.ivMatrix = glm::inverse(cloudShadowUniform.vMatrix);
                cloudShadowUniform.ipMatrix = glm::inverse(cloudShadowUniform.pMatrix);

                cloudShadowUniform.vMatrix = cloudShadowUniform.vMatrix * camera.invViewMatrix;
            }

            cloudShadowUniformBuffer.SetData(&cloudShadowUniform, 0);
            cloudShadowUniformBuffer.Bind(commandList, 3, 5);

            ivec2 res = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
            int32_t groupSize = 8;
            ivec2 groupCount = res / groupSize;
            groupCount.x += ((res.x % groupSize == 0) ? 0 : 1);
            groupCount.y += ((res.y % groupSize == 0) ? 0 : 1);

            commandList->Dispatch(groupCount.x, groupCount.y, 1);

            Graphics::Profiler::EndQuery();

        }

    }

}
