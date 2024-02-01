#include "SceneHierarchyPanel.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    void SceneHierarchyPanel::Render(Ref<Scene::Scene> &scene) {

        ImGui::Begin(GetNameID());

        isFocused = ImGui::IsWindowFocused();

        if (scene != nullptr) {

            auto root = scene->GetEntityByName("Root");

            TraverseHierarchy(root);

            if (ImGui::BeginPopupContextWindow()) {
                Scene::Entity entity;
                bool addEntity = false;
                ImGui::MenuItem("Add entity", nullptr, &addEntity);

                if (addEntity) {
                    entity = scene->CreateEntity();
                }

                if (entity.IsValid() && selectedEntity.IsValid() &&
                    selectedEntity.HasComponent<HierarchyComponent>()) {
                    auto& hierarchyComponent = selectedEntity.GetComponent<HierarchyComponent>();
                    hierarchyComponent.entities.push_back(entity);
                }
                else if (entity.IsValid()) {
                    auto& hierarchyComponent = root.GetComponent<HierarchyComponent>();
                    hierarchyComponent.entities.push_back(entity);
                }

                ImGui::EndPopup();
            }

        }

        ImGui::End();

    }

    void SceneHierarchyPanel::TraverseHierarchy(Scene::Entity entity) {

        static ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

        auto& hierarchyComponent = entity.GetComponent<HierarchyComponent>();
        auto nameComponent = entity.TryGetComponent<NameComponent>();

        std::string nodeName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

        auto nodeFlags = baseFlags;
        nodeFlags |= entity == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0;
        bool nodeOpen = ImGui::TreeNodeEx((void*)(size_t)entity, nodeFlags, "%s", nodeName.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ||
            ImGui::IsItemClicked(ImGuiMouseButton_Right) && !ImGui::IsItemToggledOpen())
            selectedEntity = entity;

        if (nodeOpen) {

            for (auto childEntity : hierarchyComponent.entities) {

                nameComponent = childEntity.TryGetComponent<NameComponent>();

                nodeFlags = baseFlags;

                if (childEntity.HasComponent<HierarchyComponent>()) {
                    TraverseHierarchy(childEntity);
                }
                else {
                    nodeName = nameComponent ? nameComponent->name : "Entity " + std::to_string(childEntity);

                    nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    nodeFlags |= childEntity == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0;

                    ImGui::TreeNodeEx((void*)(size_t)childEntity, nodeFlags, "%s", nodeName.c_str());
                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ||
                        ImGui::IsItemClicked(ImGuiMouseButton_Right) && !ImGui::IsItemToggledOpen())
                        selectedEntity = childEntity;
                }

            }

            ImGui::TreePop();

        }

    }

}