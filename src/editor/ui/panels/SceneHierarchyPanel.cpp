#include "SceneHierarchyPanel.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <ImGuizmo.h>

namespace Atlas::Editor::UI {

    void SceneHierarchyPanel::Render(Ref<Scene::Scene> &scene, bool inFocus) {

        ImGui::Begin(GetNameID());

        if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
            ImGui::SetWindowFocus();

        isFocused = ImGui::IsWindowFocused();

        if (scene != nullptr) {

            bool selectionChanged = false;

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

            ImGui::InputTextWithHint("Search", "Type to search for entity", &entitySearch);
            if (ImGui::IsItemClicked()) selectionChanged = true;

            // Search should be case-insensitive
            transformedEntitySearch = entitySearch;
            std::transform(transformedEntitySearch.begin(), transformedEntitySearch.end(),
                transformedEntitySearch.begin(), ::tolower);

            std::string nodeName;
            std::unordered_set<ECS::Entity> matchSet;
            matchSet.reserve(scene->GetEntityCount());
            if (!transformedEntitySearch.empty())
                SearchHierarchy(scene, root, matchSet, nodeName, false);

            TraverseHierarchy(scene, root, matchSet, inFocus, &selectionChanged);

            RenderExtendedHierarchy(scene, &selectionChanged);

            const auto& io = ImGui::GetIO();
            bool controlDown;
#ifdef AE_OS_MACOS
            controlDown = ImGui::IsKeyDown(ImGuiKey_LeftSuper);
#else
            controlDown = io.KeyCtrl;
#endif
            if (inFocus && controlDown && ImGui::IsKeyPressed(ImGuiKey_D, false))
                DuplicateSelectedEntity(scene);
            if (inFocus && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                DeleteSelectedEntity(scene);

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !selectionChanged) {
                selectedEntity = Scene::Entity();
                selectedProperty = SelectedProperty();
            }


        }

        ImGui::End();

    }

    void SceneHierarchyPanel::TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity,
        std::unordered_set<ECS::Entity>& matchSet, bool inFocus, bool* selectionChanged) {

        ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow |
            ImGuiTreeNodeFlags_OpenOnDoubleClick;

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        auto nameComponent = entity.TryGetComponent<NameComponent>();

        if (!hierarchyComponent) {
            baseFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        std::string nodeName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

        // If the search term matches we want to display everything below this item in the hierarchy
        bool validSearch = (transformedEntitySearch.empty() || matchSet.contains(entity));

        // If we have a search term and the name doesn't match, return
        if (nodeName != "Root" && !validSearch)
            return;

        auto nodeFlags = baseFlags;
        nodeFlags |= entity == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0;
        auto entityId = static_cast<uint64_t>(entity);
        bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(entityId), nodeFlags, "%s", nodeName.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ||
            ImGui::IsItemClicked(ImGuiMouseButton_Right) && !ImGui::IsItemToggledOpen()) {
            selectedEntity = entity;
            selectedProperty = SelectedProperty();
            *selectionChanged = true;
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
            if (ImGui::MenuItem("Delete entity"))
                deleteEntity = true;

            if (ImGui::MenuItem("Add emtpy entity"))
                createEntity = true;

            if (ImGui::MenuItem("Duplicate entity"))
                duplicateEntity = true;

            ImGui::EndPopup();
        }

        if (nodeOpen && hierarchyComponent) {

            auto children = hierarchyComponent->GetChildren();
            for (auto childEntity : children) {

                TraverseHierarchy(scene, childEntity, matchSet, inFocus, selectionChanged);

            }

            ImGui::TreePop();

        }

        if (nameComponent && nameComponent->name == "Root") {
            duplicateEntity = false;
            deleteEntity = false;
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
            // Reset other properties selection
            selectedProperty = SelectedProperty();
        }

        if (deleteEntity) {
            DeleteSelectedEntity(scene);
        }

        if (duplicateEntity) {
            DuplicateSelectedEntity(scene);
        }

        *selectionChanged |= createEntity | deleteEntity | duplicateEntity;

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
        hierarchyComponent = deleteEntity ? nullptr : entity.TryGetComponent<HierarchyComponent>();
        if (hierarchyComponent && hierarchyComponent->GetChildren().empty()) {
            entity.RemoveComponent<HierarchyComponent>();
        }

    }

    void SceneHierarchyPanel::RenderExtendedHierarchy(const Ref<Scene::Scene>& scene, bool* selectionChanged) {

        ImGui::Separator();

        if (scene->fog)
            RenderExtendedItem("Fog", &selectedProperty.fog, selectionChanged);
        if (scene->sky.clouds)
            RenderExtendedItem("Volumetric clouds", &selectedProperty.volumetricClouds, selectionChanged);
        if (scene->irradianceVolume)
            RenderExtendedItem("Irradiance volume", &selectedProperty.irradianceVolume, selectionChanged);
        if (scene->reflection)
            RenderExtendedItem("Reflection", &selectedProperty.reflection, selectionChanged);
        if (scene->ssgi)
            RenderExtendedItem("Screen-space global illumination", &selectedProperty.ssgi, selectionChanged);
        if (scene->sss)
            RenderExtendedItem("Screen-space shadows", &selectedProperty.sss, selectionChanged);

        RenderExtendedItem("Wind", &selectedProperty.wind, selectionChanged);
        RenderExtendedItem("Post processing", &selectedProperty.postProcessing, selectionChanged);

    }

    void SceneHierarchyPanel::RenderExtendedItem(const std::string &name, bool *selected, bool *selectionChanged) {

        ImGuiTreeNodeFlags nodeFlags =  ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

        nodeFlags |= *selected ? ImGuiTreeNodeFlags_Selected : 0;
        ImGui::TreeNodeEx(name.c_str(), nodeFlags);
        if (ImGui::IsItemClicked()) {
            selectedProperty = SelectedProperty();
            selectedEntity = Scene::Entity();
            *selected = true;
            *selectionChanged = true;
        }

    }

    void SceneHierarchyPanel::DeleteSelectedEntity(Ref<Scene::Scene>& scene) {

        if (selectedEntity.IsValid())
            scene->DestroyEntity(selectedEntity);

        selectedEntity = Scene::Entity();
        // Reset other properties selection
        selectedProperty = SelectedProperty();

    }

    void SceneHierarchyPanel::DuplicateSelectedEntity(Ref<Scene::Scene>& scene) {

        if (!selectedEntity.IsValid())
            return;

        auto parentEntity = scene->GetParentEntity(selectedEntity);

        // Create new hierarchy before retrieving other components since they might become
        // invalid when internal ECS storage resizes
        auto newEntity = scene->DuplicateEntity(selectedEntity);

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

        selectedEntity = newEntity;
        // Reset other properties selection
        selectedProperty = SelectedProperty();

    }

    bool SceneHierarchyPanel::SearchHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity, 
        std::unordered_set<ECS::Entity>& matchSet, std::string& nodeName, bool parentMatches) {

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        auto nameComponent = entity.TryGetComponent<NameComponent>();

        // Only allocate a new string when there is no name component
        if (nameComponent) {
            nodeName = nameComponent->name;
        }
        else {
            nodeName = "entity " + std::to_string(entity);
        }

        auto it = std::search(nodeName.begin(), nodeName.end(), transformedEntitySearch.begin(), transformedEntitySearch.end(),
            [](unsigned char nodeChar, unsigned char searchChar) { return std::tolower(nodeChar) == searchChar; });

        parentMatches |= it != nodeName.end();
        bool matches = parentMatches;

        if (hierarchyComponent) {

            auto children = hierarchyComponent->GetChildren();
            for (auto childEntity : children) {

                matches |= SearchHierarchy(scene, childEntity, matchSet, nodeName, parentMatches);

            }

        }

        if (matches)
            matchSet.insert(entity);

        return matches;

    }

}