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

            if (entity.HasComponent<AudioComponent>()) {
                auto& comp = entity.GetComponent<AudioComponent>();
                RenderComponentPanel("Audio component", scene,
                    entity, audioComponentPanel, comp);
            }

            if (entity.HasComponent<AudioVolumeComponent>()) {
                auto& comp = entity.GetComponent<AudioVolumeComponent>();
                RenderComponentPanel("Audio volume component", scene,
                    entity, audioVolumeComponentPanel, comp);
            }

            if (entity.HasComponent<CameraComponent>()) {
                auto& comp = entity.GetComponent<CameraComponent>();
                RenderComponentPanel("Camera component", scene,
                    entity, cameraComponentPanel, comp);
            }

            if (entity.HasComponent<TextComponent>()) {
                auto& comp = entity.GetComponent<TextComponent>();
                RenderComponentPanel("Text component", scene,
                    entity, textComponentPanel, comp);
            }

            if (entity.HasComponent<LuaScriptComponent>()) {
                RenderComponentPanel("Lua script component", scene, 
                    entity, luaScriptComponentPanel, entity.GetComponent<LuaScriptComponent>());
            }

            if (entity.HasComponent<RigidBodyComponent>()) {
                auto comp = entity.GetComponent<RigidBodyComponent>();
                RenderComponentPanel("Rigid body component", scene,
                    entity, rigidBodyComponentPanel, comp);
                // Only change if we have new settings
                if (comp.creationSettings) {
                    entity.RemoveComponent<RigidBodyComponent>();
                    entity.AddComponent<RigidBodyComponent>(*comp.creationSettings);
                }
            }

            if (entity.HasComponent<PlayerComponent>()) {
                auto comp = entity.GetComponent<PlayerComponent>();
                RenderComponentPanel("Player component", scene,
                    entity, playerComponentPanel, comp);
                // Only change if we have new settings
                entity.RemoveComponent<PlayerComponent>();
                auto& newComp = entity.AddComponent<PlayerComponent>(*comp.creationSettings);
                newComp.slowVelocity = comp.slowVelocity;
                newComp.fastVelocity = comp.fastVelocity;
                newComp.jumpVelocity = comp.jumpVelocity;
            }
        }

        // Add components
        {
            if (ImGui::Button("Add component", { -FLT_MIN, 0 }))
                ImGui::OpenPopup("AddComponent");

            if (ImGui::BeginPopup("AddComponent")) {
                if (!entity.HasComponent<NameComponent>() && ImGui::MenuItem("Add name component"))
                    entity.AddComponent<NameComponent>("Entity " + std::to_string(entity));
                if (!entity.HasComponent<TransformComponent>() && ImGui::MenuItem("Add transform component"))
                    entity.AddComponent<TransformComponent>(mat4(1.0f), false);
                if (!entity.HasComponent<MeshComponent>() && ImGui::MenuItem("Add mesh component"))
                    entity.AddComponent<MeshComponent>();
                if (!entity.HasComponent<AudioComponent>() && ImGui::MenuItem("Add audio component"))
                    entity.AddComponent<AudioComponent>();
                if (!entity.HasComponent<AudioVolumeComponent>() && ImGui::MenuItem("Add audio volume component"))
                    entity.AddComponent<AudioVolumeComponent>();
                if (!entity.HasComponent<CameraComponent>() && ImGui::MenuItem("Add camera component"))
                    entity.AddComponent<CameraComponent>();
                if (!entity.HasComponent<TextComponent>() && ImGui::MenuItem("Add text component"))
                    entity.AddComponent<TextComponent>();
				if (!entity.HasComponent<LuaScriptComponent>() && ImGui::MenuItem("Add lua script component"))
                    entity.AddComponent<LuaScriptComponent>();

                // Just make the player component addable if there is a transform component
                if (entity.HasComponent<TransformComponent>() &&
                    !entity.HasComponent<PlayerComponent>() && ImGui::MenuItem("Add player component")) {
                    // Created standardized shapes/rigid body component as a default (we need to get the scale from
                    // the transform component
                    vec3 scale = vec3(1.0f);
                    scale = entity.GetComponent<TransformComponent>().Decompose().scale;

                    auto shape = Physics::ShapesManager::CreateShape(Physics::CapsuleShapeSettings{ .scale = scale });
                    auto bodySettings = Physics::PlayerCreationSettings { .shape = shape };
                    entity.AddComponent<PlayerComponent>(bodySettings);
                }

                // Just make the rigid body component addable if there is a transform component
                if (entity.HasComponent<TransformComponent>() &&
                    !entity.HasComponent<RigidBodyComponent>() && ImGui::MenuItem("Add rigid body component")) {
                    // Created standardized shapes/rigid body component as a default (we need to get the scale from
                    // the transform component
                    vec3 scale = vec3(1.0f);
                    auto& globalMatrix = entity.GetComponent<TransformComponent>().globalMatrix;
                    // Need scale from global matrix decomposition
                    scale = Common::MatrixDecomposition(globalMatrix).scale;

                    auto shape = Physics::ShapesManager::CreateShape(Physics::BoundingBoxShapeSettings { .scale = scale });
                    auto bodySettings = Physics::BodyCreationSettings { .objectLayer = Physics::Layers::MOVABLE, .shape = shape };
                    entity.AddComponent<RigidBodyComponent>(bodySettings);
                }

                ImGui::EndPopup();
            }
        }

        // Remove components
        {
            if (ImGui::Button("Remove component", { -FLT_MIN, 0 }))
                ImGui::OpenPopup("RemoveComponent");

            if (ImGui::BeginPopup("RemoveComponent")) {
                if (entity.HasComponent<NameComponent>() && ImGui::MenuItem("Remove name component"))
                    entity.RemoveComponent<NameComponent>();
                if (entity.HasComponent<TransformComponent>() && ImGui::MenuItem("Remove transform component"))
                    entity.RemoveComponent<TransformComponent>();
                if (entity.HasComponent<MeshComponent>() && ImGui::MenuItem("Remove mesh component"))
                    entity.RemoveComponent<MeshComponent>();
                if (entity.HasComponent<AudioComponent>() && ImGui::MenuItem("Remove audio component"))
                    entity.RemoveComponent<AudioComponent>();
                if (entity.HasComponent<AudioVolumeComponent>() && ImGui::MenuItem("Remove audio volume component"))
                    entity.RemoveComponent<AudioVolumeComponent>();
                if (entity.HasComponent<CameraComponent>() && ImGui::MenuItem("Remove camera component"))
                    entity.RemoveComponent<CameraComponent>();
                if (entity.HasComponent<TextComponent>() && ImGui::MenuItem("Remove text component"))
                    entity.RemoveComponent<TextComponent>();
                if (entity.HasComponent<LuaScriptComponent>() && ImGui::MenuItem("Remove lua script component"))
                    entity.RemoveComponent<LuaScriptComponent>();
                if (entity.HasComponent<PlayerComponent>() && ImGui::MenuItem("Remove player component")) 
                    entity.RemoveComponent<PlayerComponent>();
                if (entity.HasComponent<RigidBodyComponent>() && ImGui::MenuItem("Remove rigid body component")) 
                    entity.RemoveComponent<RigidBodyComponent>();

                ImGui::EndPopup();
            }
        }

    }

}