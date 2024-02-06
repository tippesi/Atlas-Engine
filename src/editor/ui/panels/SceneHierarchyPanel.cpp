#include "SceneHierarchyPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    void SceneHierarchyPanel::Render(Ref<Scene::Scene> &scene) {

        ImGui::Begin(GetNameID());

        isFocused = ImGui::IsWindowFocused();

        if (scene != nullptr) {

            auto root = scene->GetEntityByName("Root");

            if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
                // Unselect entity in this case, since this popup could only be opened by clicking on emtpy space
                selectedEntity = Scene::Entity();

                Scene::Entity entity;
                if (ImGui::MenuItem("Add emtpy entity"))
                    entity = scene->CreateEntity();

                if (entity.IsValid()) {
                    auto &hierarchyComponent = root.GetComponent<HierarchyComponent>();
                    hierarchyComponent.AddChild(entity);

                    selectedEntity = entity;
                }

                ImGui::EndPopup();
            }

            TraverseHierarchy(scene, root);

        }

        ImGui::End();

    }

    void SceneHierarchyPanel::TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity) {

        ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick;

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        auto nameComponent = entity.TryGetComponent<NameComponent>();

        if (!hierarchyComponent) {
            baseFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        std::string nodeName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

        auto nodeFlags = baseFlags;
        nodeFlags |= entity == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0;
        bool nodeOpen = ImGui::TreeNodeEx((void*)(size_t)entity, nodeFlags, "%s", nodeName.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ||
            ImGui::IsItemClicked(ImGuiMouseButton_Right) && !ImGui::IsItemToggledOpen())
            selectedEntity = entity;

        bool deleteEntity = false;
        if (ImGui::BeginPopupContextItem()) {
            Scene::Entity newEntity;

            // We shouldn't allow the user to delete the root entity
            if (ImGui::MenuItem("Delete entity") && (!nameComponent || nameComponent->name != "Root"))
                deleteEntity = true;

            if (ImGui::MenuItem("Add emtpy entity"))
                newEntity = scene->CreateEntity();

            if (newEntity.IsValid()) {
                // No hierarchy component, so create one
                if (!hierarchyComponent) {
                    entity.AddComponent<HierarchyComponent>();
                    hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
                }

                hierarchyComponent->AddChild(newEntity);

                selectedEntity = newEntity;
            }

            if (ImGui::MenuItem("Duplicate entity")) {
                auto parentEntity = scene->GetParentEntity(entity);

                // Create new hierarchy before retrieving other components since they might become
                // invalid when internal ECS storage resizes
                auto newEntity = scene->DuplicateEntity(entity);

                HierarchyComponent* component;
                if (parentEntity.IsValid()) {
                    component = parentEntity.TryGetComponent<HierarchyComponent>();
                }
                else {
                    auto root = scene->GetEntityByName("Root");
                    component = root.TryGetComponent<HierarchyComponent>();
                }

                AE_ASSERT(component != nullptr);

                if (component != nullptr) {
                    
                    component->AddChild(newEntity);
                }
            }

            ImGui::EndPopup();
        }

        if (nodeOpen && hierarchyComponent) {

            for (auto childEntity : hierarchyComponent->GetChildren()) {

                TraverseHierarchy(scene, childEntity);

            }

            ImGui::TreePop();

        }

        if (deleteEntity) {
            scene->DestroyEntity(entity);
        }

    }

}