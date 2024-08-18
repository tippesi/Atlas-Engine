#include "LightData.h"
#include "CommonStructures.h"

#include "common/ColorConverter.h"
#include <glm/gtx/norm.hpp>

namespace Atlas::Renderer::Helper {

    void LightData::CullAndSort(const Ref<Scene::Scene>& scene) {

        auto& camera = scene->GetMainCamera();

        lightEntities.clear();
        auto lightSubset = scene->GetSubset<LightComponent>();
        for (auto& lightEntity : lightSubset) {
            auto& light = lightEntity.GetComponent<LightComponent>();
            lightEntities.emplace_back(LightEntity { lightEntity, light, -1 });
        }

        if (lightEntities.size() <= 1)  
            return;

        std::sort(lightEntities.begin(), lightEntities.end(),
            [&](const LightEntity& light0, const LightEntity& light1) {
                if (light0.comp.isMain)
                    return true;

                if (light0.comp.type == LightType::DirectionalLight)
                    return true;

                if (light0.comp.type == LightType::PointLight &&
                    light1.comp.type == LightType::PointLight) {
                    return glm::distance2(light0.comp.transformedProperties.point.position, camera.GetLocation())
                        < glm::distance2(light1.comp.transformedProperties.point.position, camera.GetLocation());
                }

                return false;
            });

    }

    void LightData::UpdateBindlessIndices(const Ref<Scene::Scene>& scene) {



    }

    void LightData::FillBuffer(const Ref<Scene::Scene>& scene) {

        auto& camera = scene->GetMainCamera();

        std::vector<Light> lights;
        lights.reserve(lightEntities.size());
        for (const auto& entity : lightEntities) {
            auto& light = entity.comp;

            auto type = static_cast<uint32_t>(light.type);
            auto packedType = reinterpret_cast<float&>(type);
            Light lightUniform {
                .color = vec4(Common::ColorConverter::ConvertSRGBToLinear(light.color), packedType),
                .intensity = light.intensity,
                .scatteringFactor = 1.0f,
            };

            const auto& prop = light.transformedProperties;
            if (light.type == LightType::DirectionalLight) {
                lightUniform.direction = camera.viewMatrix * vec4(prop.directional.direction, 0.0f);
            }
            else if (light.type == LightType::PointLight) {
                lightUniform.location = camera.viewMatrix * vec4(prop.point.position, 1.0f);
                lightUniform.radius = prop.point.radius;
                lightUniform.attenuation = prop.point.attenuation;
            }

            auto& shadowUniform = lightUniform.shadow;
            if (light.shadow) {
                auto shadow = light.shadow;
                auto& shadowUniform = lightUniform.shadow;
                shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                shadowUniform.bias = shadow->bias;
                shadowUniform.edgeSoftness = shadow->edgeSoftness;
                shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                shadowUniform.cascadeCount = shadow->viewCount;
                shadowUniform.resolution = vec2(shadow->resolution);
                shadowUniform.mapIdx = entity.mapIdx;

                auto componentCount = shadow->viewCount;
                for (int32_t i = 0; i < MAX_SHADOW_VIEW_COUNT + 1; i++) {
                    if (i < componentCount) {
                        auto cascade = &shadow->views[i];
                        auto frustum = Volume::Frustum(cascade->frustumMatrix);
                        auto corners = frustum.GetCorners();
                        auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                            abs(corners[1].y - corners[3].y)) / (float)shadow->resolution;
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                        if (light.type == LightType::DirectionalLight) {
                            shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix *
                                cascade->viewMatrix * camera.invViewMatrix;
                        }
                        else if (light.type == LightType::PointLight) {
                            if (i == 0)
                                shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix;
                            else
                                shadowUniform.cascades[i].cascadeSpace = glm::translate(mat4(1.0f), -prop.point.position) * camera.invViewMatrix;
                        }
                        shadowUniform.cascades[i].texelSize = texelSize;
                    }
                    else {
                        auto cascade = &shadow->views[componentCount - 1];
                        shadowUniform.cascades[i].distance = cascade->farDistance;
                    }
                }
            }

            lights.emplace_back(lightUniform);
        }

        if (lightBuffer.GetElementCount() < lightEntities.size()) {
            lightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::HostAccessBit | Buffer::BufferUsageBits::MultiBufferedBit
                | Buffer::BufferUsageBits::StorageBufferBit, sizeof(Light), lightEntities.size(), lights.data());
        }
        else {
            lightBuffer.SetData(lights.data(), 0, lights.size());
        }

    }

}