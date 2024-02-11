#include "EntityPropertiesPanel.h"

namespace Atlas::Editor::UI {

    void EntityPropertiesPanel::Render(Ref<Scene::Scene>& scene, Scene::Entity entity) {

        if (!entity.IsValid())
            return;

        auto nameComponent = entity.TryGetComponent<NameComponent>();
        if (nameComponent) {
            nameComponentPanel.Render(scene, entity, *nameComponent);
        }

        // General components
        {
            if (entity.HasComponent<TransformComponent>()) {
                RenderComponentPanel("Transform component", scene,
                    entity, transformComponentPanel, entity.GetComponent<TransformComponent>());
            }

            if (entity.HasComponent<MeshComponent>()) {
                // Create a copy here to be able to change the resource
                auto comp = entity.GetComponent<MeshComponent>();
                RenderComponentPanel("Mesh component", scene,
                    entity, meshComponentPanel, comp);
                entity.RemoveComponent<MeshComponent>();
                entity.AddComponent<MeshComponent>(comp);
            }

            if (entity.HasComponent<LightComponent>()) {
                RenderComponentPanel("Light component", scene, entity,
                    lightComponentPanel, entity.GetComponent<LightComponent>());
            }

            if (entity.HasComponent<AudioVolumeComponent>()) {
                auto comp = entity.GetComponent<AudioVolumeComponent>();
                RenderComponentPanel("Audio volume component", scene,
                    entity, audioVolumeComponentPanel, comp);
                entity.RemoveComponent<AudioVolumeComponent>();
                entity.AddComponent<AudioVolumeComponent>(comp);
            }

            if (entity.HasComponent<RigidBodyComponent>()) {
                auto comp = entity.GetComponent<RigidBodyComponent>();
                RenderComponentPanel("Rigid body component", scene,
                    entity, rigidBodyComponentPanel, comp);
                // Only change if we have new settings
                if (comp.bodyCreationSettings) {
                    entity.RemoveComponent<RigidBodyComponent>();
                    entity.AddComponent<RigidBodyComponent>(*comp.bodyCreationSettings);
                }
            }

            if (entity.HasComponent<CameraComponent>()) {
                auto& comp = entity.GetComponent<CameraComponent>();
                RenderComponentPanel("Camera component", scene,
                    entity, cameraComponentPanel, comp);
            }
        }

        // Add components
        {
            if (ImGui::Button("Add component", { -FLT_MIN, 0 }))
                ImGui::OpenPopup("NewComponent");

            if (ImGui::BeginPopup("NewComponent")) {
                if (!entity.HasComponent<NameComponent>() && ImGui::MenuItem("Add name component"))
                    entity.AddComponent<NameComponent>("Entity " + std::to_string(entity));
                if (!entity.HasComponent<TransformComponent>() && ImGui::MenuItem("Add transform component"))
                    entity.AddComponent<TransformComponent>(mat4(1.0f), false);
                if (!entity.HasComponent<MeshComponent>() && ImGui::MenuItem("Add mesh component"))
                    entity.AddComponent<MeshComponent>();
                if (!entity.HasComponent<AudioVolumeComponent>() && ImGui::MenuItem("Add audio volume component"))
                    entity.AddComponent<AudioVolumeComponent>();
                if (!entity.HasComponent<CameraComponent>() && ImGui::MenuItem("Add camera component"))
                    entity.AddComponent<CameraComponent>();

                // Just make the rigid body component addable if there is a transform component
                if (entity.HasComponent<TransformComponent>() &&
                    !entity.HasComponent<RigidBodyComponent>() && ImGui::MenuItem("Add rigid body component")) {
                    // Created standardized shapes/rigid body component as a default (we need to get the scale from
                    // the transform component
                    vec3 scale = vec3(1.0f);
                    scale = entity.GetComponent<TransformComponent>().Decompose().scale;

                    auto shape = Physics::ShapesManager::CreateShape(Physics::BoundingBoxShapeSettings { .scale = scale });
                    auto bodySettings = Physics::BodyCreationSettings { .objectLayer = Physics::Layers::MOVABLE, .shape = shape };
                    entity.AddComponent<RigidBodyComponent>(bodySettings);
                }

                ImGui::EndPopup();
            }
        }

    }

}