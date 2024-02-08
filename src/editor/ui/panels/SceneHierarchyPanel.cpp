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
                    entity.AddComponent<NameComponent>("Entity " + std::to_string(entity));

                    auto &hierarchyComponent = root.GetComponent<HierarchyComponent>();
                    hierarchyComponent.AddChild(entity);

                    selectedEntity = entity;
                }

                ImGui::EndPopup();
            }

            TraverseHierarchy(scene, root);

            RenderExtendedHierarchy(scene);

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
        auto entityId = static_cast<uint64_t>(entity);
        bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(entityId), nodeFlags, "%s", nodeName.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ||
            ImGui::IsItemClicked(ImGuiMouseButton_Right) && !ImGui::IsItemToggledOpen()) {
            selectedEntity = entity;
            selectedProperty = SelectedProperty();
        }

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(typeid(Scene::Entity).name(), &entity, sizeof(Scene::Entity));
            ImGui::Text("Drag to other entity in hierarchy");

            ImGui::EndDragDropSource();
        }

        Scene::Entity dropEntity;
        if (ImGui::BeginDragDropTarget()) {
            auto dropPayload = ImGui::GetDragDropPayload();
            if (dropPayload->IsDataType(typeid(Scene::Entity).name())) {
                std::memcpy(&dropEntity, dropPayload->Data, dropPayload->DataSize);
                if (entity == dropEntity || !ImGui::AcceptDragDropPayload(typeid(Scene::Entity).name())) {
                    dropEntity = Scene::Entity();
                }
            }

            ImGui::EndDragDropTarget();
        }

        bool createEntity = false;
        bool deleteEntity = false;
        bool duplicateEntity = false;
        if (ImGui::BeginPopupContextItem()) {
            // We shouldn't allow the user to delete the root entity
            if (ImGui::MenuItem("Delete entity") && (!nameComponent || nameComponent->name != "Root"))
                deleteEntity = true;

            if (ImGui::MenuItem("Add emtpy entity"))
                createEntity = true;

            if (ImGui::MenuItem("Duplicate entity") && (!nameComponent || nameComponent->name != "Root"))
                duplicateEntity = true;

            ImGui::EndPopup();
        }

        if (nodeOpen && hierarchyComponent) {

            auto children = hierarchyComponent->GetChildren();
            for (auto childEntity : children) {

                TraverseHierarchy(scene, childEntity);

            }

            ImGui::TreePop();

        }

        if (createEntity) {
            auto newEntity = scene->CreateEntity();
            newEntity.AddComponent<NameComponent>("Entity " + std::to_string(newEntity));

            // No hierarchy component, so create one
            if (!hierarchyComponent) {
                entity.AddComponent<HierarchyComponent>();
                hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
            }

            hierarchyComponent->AddChild(newEntity);

            selectedEntity = newEntity;
        }

        if (deleteEntity) {
            scene->DestroyEntity(entity);
        }

        if (duplicateEntity) {
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

        if (dropEntity.IsValid()) {
            auto dropParentEntity = scene->GetParentEntity(dropEntity);

            if (dropParentEntity.IsValid()) {
                auto& dropParentHierarchy = dropParentEntity.GetComponent<HierarchyComponent>();
                dropParentHierarchy.RemoveChild(dropEntity);
            }

            // No hierarchy component, so create one
            if (!hierarchyComponent) {
                entity.AddComponent<HierarchyComponent>();
                hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
            }

            hierarchyComponent->AddChild(dropEntity);
        }

        // If the hierarchy is emtpy after movements or deletions, also remove the hierarchy
        if (hierarchyComponent && hierarchyComponent->GetChildren().empty()) {
            entity.RemoveComponent<HierarchyComponent>();
        }

    }

    void SceneHierarchyPanel::RenderExtendedHierarchy(Ref<Scene::Scene>& scene) {

        ImGui::Separator();

        RenderExtendedItem("Fog", &selectedProperty.fog);
        RenderExtendedItem("Volumetric clouds", &selectedProperty.volumetricClouds);
        RenderExtendedItem("Post processing", &selectedProperty.postProcessing);

    }

    void SceneHierarchyPanel::RenderExtendedItem(const std::string &name, bool *selected) {

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        nodeFlags |= *selected ? ImGuiTreeNodeFlags_Selected : 0;
        ImGui::TreeNodeEx(name.c_str(), nodeFlags);
        if (ImGui::IsItemClicked()) {
            selectedProperty = SelectedProperty();
            selectedEntity = Scene::Entity();
            *selected = true;
        }

    }

}