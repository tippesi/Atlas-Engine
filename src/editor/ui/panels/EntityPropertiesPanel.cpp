#include "EntityPropertiesPanel.h"

namespace Atlas::Editor::UI {

    void EntityPropertiesPanel::Render(Scene::Entity entity) {

        if (!entity.IsValid())
            return;

        auto nameComponent = entity.TryGetComponent<NameComponent>();
        if (nameComponent) {
            nameComponentPanel.Render(*nameComponent);
        }

        if (entity.HasComponent<TransformComponent>()) {
            RenderComponentPanel("Transform component", transformComponentPanel,
                entity.GetComponent<TransformComponent>());
        }

        if (entity.HasComponent<MeshComponent>()) {
            // Create a copy here to be able to change the resource
            auto comp = entity.GetComponent<MeshComponent>();
            bool resourceChanged = RenderComponentPanel("Mesh component", meshComponentPanel,
                comp);
            // We need to replace the component such that the scene is informed about the resource change
            if (resourceChanged) {
                entity.ReplaceComponent<MeshComponent>(comp);
            }
        }

        if (entity.HasComponent<LightComponent>()) {
            RenderComponentPanel("Light component", lightComponentPanel,
                entity.GetComponent<LightComponent>());
        }

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