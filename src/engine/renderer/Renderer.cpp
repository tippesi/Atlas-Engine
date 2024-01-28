#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        Scene::Entity Renderer::GetMainLightEntity(Ref<Scene::Scene>& scene) {

            Scene::Entity entity;

            Scene::Entity mainLightEntity;
            auto lightSubset = scene->GetSubset<LightComponent>();

            // Currently the renderers just support one main directional light
            for (auto& lightEntity : lightSubset) {
                auto &light = lightEntity.GetComponent<LightComponent>();

                if (light.isMain && light.type == LightType::DirectionalLight) {
                    mainLightEntity = lightEntity;
                    break;
                }
            }

            return entity;

        }

    }

}