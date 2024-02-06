#include "EntityPropertiesPanel.h"

namespace Atlas::Editor::UI {

    void EntityPropertiesPanel::Render(Scene::Entity entity) {

        if (!entity.IsValid())
            return;

        auto nameComponent = entity.TryGetComponent<NameComponent>();
        if (nameComponent) {
            nameComponentPanel.Render(entity, *nameComponent);
        }

        // General components
        {
            if (entity.HasComponent<TransformComponent>()) {
                RenderComponentPanel("Transform component", entity,
                    transformComponentPanel, entity.GetComponent<TransformComponent>());
            }

            if (entity.HasComponent<MeshComponent>()) {
                // Create a copy here to be able to change the resource
                auto comp = entity.GetComponent<MeshComponent>();
                RenderComponentPanel("Mesh component",  entity,
                    meshComponentPanel, comp);
                entity.RemoveComponent<MeshComponent>();
                entity.AddComponent<MeshComponent>(comp);
            }

            if (entity.HasComponent<LightComponent>()) {
                RenderComponentPanel("Light component", entity,
                    lightComponentPanel, entity.GetComponent<LightComponent>());
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

                ImGui::EndPopup();
            }
        }

    }

}