#include "SceneHierarchyPanel.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <ImGuizmo.h>

namespace Atlas::Editor::UI {

    void SceneHierarchyPanel::Update(Ref<Scene::Scene>& scene) {

        if (Singletons::blockingOperation->block)
            return;

        if (!searchJob.HasFinished())
            return;

        matchSet = newMatchSet;

        if (entitySearch == lastEntitySearch && lastAliveEntityCount == scene->GetEntityCount() && !hierarchyFilterChanged)
            return;

        lastEntitySearch = entitySearch;
        lastAliveEntityCount = scene->GetEntityCount();
        hierarchyFilterChanged = false;

        EntityData data{
            .hierarchyPool = scene->entityManager.GetPool<HierarchyComponent>(),
            .namePool = scene->entityManager.GetPool<NameComponent>(),
            .transformPool = scene->entityManager.GetPool<TransformComponent>(),
            .meshPool = scene->entityManager.GetPool<MeshComponent>(),
            .textPool = scene->entityManager.GetPool<TextComponent>(),
            .audioPool = scene->entityManager.GetPool<AudioComponent>(),
            .audioVolumePool = scene->entityManager.GetPool<AudioVolumeComponent>(),
            .playerPool = scene->entityManager.GetPool<PlayerComponent>(),
            .rigidBodyPool = scene->entityManager.GetPool<RigidBodyComponent>(),
            .cameraPool = scene->entityManager.GetPool<CameraComponent>(),
            .scriptPool = scene->entityManager.GetPool<LuaScriptComponent>(),
            .lightPool = scene->entityManager.GetPool<LightComponent>(),
        };

        auto root = scene->GetEntityByName("Root");
        // Aquire scene here such that it can't be freed and copy important pools
        JobSystem::Execute(searchJob, [&, scene = scene, root = root, data = std::move(data)](JobData&) mutable {
           
            // Search should be case-insensitive
            transformedEntitySearch = entitySearch;
            std::transform(transformedEntitySearch.begin(), transformedEntitySearch.end(),
                transformedEntitySearch.begin(), ::tolower);
            
            newMatchSet.clear();
            newMatchSet.reserve(scene->entityManager.Alive());
            std::string nodeName;
            SearchHierarchy(data, root, newMatchSet, nodeName, false);
            });

    }

    void SceneHierarchyPanel::Render(Ref<Scene::Scene> &scene, bool inFocus) {

        const float padding = 8.0f;

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

            std::string prevEntitySearch = entitySearch;

            auto region = ImGui::GetContentRegionAvail();
            auto lineHeight = ImGui::GetTextLineHeight();
            auto buttonSize = ImVec2(lineHeight, lineHeight);

            // Wrap input text field into child, such that we can adjust the total size to not overlap anything
            ImGui::BeginChild("Search field", ImVec2(region.x - (buttonSize.x + padding), lineHeight + padding));
            ImGui::InputTextWithHint("Search", "Type to search for entity", &entitySearch);
            bool searchChanged = entitySearch != prevEntitySearch;
            if (ImGui::IsItemClicked()) selectionChanged = true;
            ImGui::EndChild();

            RenderFilterPopup();

            TraverseHierarchy(scene, root, matchSet, inFocus, searchChanged, &selectionChanged);

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
            if (inFocus && !io.WantCaptureKeyboard && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                DeleteSelectedEntity(scene);

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !selectionChanged) {
                selectedEntity = Scene::Entity();
                selectedProperty = SelectedProperty();
            }


        }

        ImGui::End();

    }

    void SceneHierarchyPanel::TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity,
        std::unordered_set<ECS::Entity>& matchSet, bool inFocus, bool searchChanged, bool* selectionChanged) {

        ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        auto nameComponent = entity.TryGetComponent<NameComponent>();

        if (!hierarchyComponent) {
            baseFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        std::string nodeName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

        // If the search term matches we want to display everything below this item in the hierarchy
        bool validSearch = matchSet.contains(entity);

        bool rootNode = nodeName == "Root";

        // If we have a search term and the name doesn't match, return
        if (!rootNode && (!validSearch || !entity.IsValid()))
            return;

        auto lineHeight = ImGui::GetTextLineHeightWithSpacing();
        auto rectWidth = ImGui::GetContentRegionAvail().x;
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        bool isItemVisible = ImGui::IsRectVisible(cursorPos, ImVec2(cursorPos.x + rectWidth, cursorPos.y + lineHeight));

        if (matchSet.contains(entity) && !entitySearch.empty() && searchChanged)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        else if (entitySearch.empty() && searchChanged || !matchSet.contains(entity) && searchChanged)
            ImGui::SetNextItemOpen(false, ImGuiCond_Always);

        auto nodeFlags = baseFlags;
        nodeFlags |= entity == selectedEntity ? ImGuiTreeNodeFlags_Selected : 0;

        Scene::Entity dropEntity;
        bool nodeOpen = false;
        bool createEntity = false;
        bool deleteEntity = false;
        bool duplicateEntity = false;

        // Don't waste precious CPU cycles here if not visible
        if (isItemVisible) {            
            auto entityId = static_cast<uint64_t>(entity);
            nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(entityId), nodeFlags, "%s", nodeName.c_str());
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
           
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Add emtpy entity"))
                    createEntity = true;

                if (!rootNode && ImGui::MenuItem("Duplicate entity"))
                    duplicateEntity = true;

                // We shouldn't allow the user to delete the root entity
                if (!rootNode && ImGui::MenuItem("Delete entity"))
                    deleteEntity = true;

                ImGui::EndPopup();
            }

            ImGui::SameLine();

            bool isNodeVisible = !nodeInvisibleSet.contains(entity);
            auto visibilityIcon = Singletons::icons->Get(IconType::Visibility);
            auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&visibilityIcon);

            float buttonSize = ImGui::GetTextLineHeight();
            auto width = ImGui::GetContentRegionAvail().x;
            auto pos = ImGui::GetCursorPosX();

            ImGui::SetCursorPosX(pos + width - buttonSize);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));

            if (ImGui::ImageButton(set, ImVec2(buttonSize, buttonSize), ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
                isNodeVisible = !isNodeVisible;

                ToggleHierarchyVisibility(entity, isNodeVisible);

                // We are checking the new state here
                if (!isNodeVisible)
                    nodeInvisibleSet.insert(entity);
                else
                    nodeInvisibleSet.erase(entity);
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }
        else {
            // Still need to display something here
            auto entityId = static_cast<uint64_t>(entity);
            nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(entityId), nodeFlags, "");
        }

        if (nodeOpen && hierarchyComponent) {

            auto children = hierarchyComponent->GetChildren();
            for (auto childEntity : children) {

                TraverseHierarchy(scene, childEntity, matchSet, inFocus, searchChanged, selectionChanged);

            }

            ImGui::TreePop();

        }

        if (rootNode) {
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

        RenderExtendedItem("Terrain", &selectedProperty.terrain, selectionChanged);
        if (scene->irradianceVolume)
            RenderExtendedItem("Irradiance volume", &selectedProperty.irradianceVolume, selectionChanged);
        if (scene->rtgi)
            RenderExtendedItem("Raytraced global illumination", &selectedProperty.rtgi, selectionChanged);
        if (scene->ssgi)
            RenderExtendedItem("Screen-space global illumination", &selectedProperty.ssgi, selectionChanged);
        if (scene->reflection)
            RenderExtendedItem("Reflection", &selectedProperty.reflection, selectionChanged);
        if (scene->sss)
            RenderExtendedItem("Screen-space shadows", &selectedProperty.sss, selectionChanged);
        if (scene->sky.clouds)
            RenderExtendedItem("Volumetric clouds", &selectedProperty.volumetricClouds, selectionChanged);
        if (scene->fog)
            RenderExtendedItem("Fog", &selectedProperty.fog, selectionChanged);        
        RenderExtendedItem("Sky", &selectedProperty.sky, selectionChanged);
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

    void SceneHierarchyPanel::RenderFilterPopup() {

        const float padding = 8.0f;

        ImGui::SameLine();

        auto region = ImGui::GetContentRegionAvail();
        auto lineHeight = ImGui::GetTextLineHeight();
        auto buttonSize = ImVec2(lineHeight, lineHeight);

        auto& filterIcon = Singletons::icons->Get(IconType::Filter);
        auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&filterIcon);

        auto uvMin = ImVec2(0.1f, 0.1f);
        auto uvMax = ImVec2(0.9f, 0.9f);

        //ImGui::SetCursorPosX(ImVec2(region.x - (buttonSize.x + 2.0f * padding), ImGui::GetCursorPosY()));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax)) {
            ImGui::OpenPopup("Hierarchy filter settings");
        }
        ImGui::PopStyleColor();

        auto renderSingleBitOption = [&](const char* name, HierarchyFilterBits bit) {
            bool isSet = hierarchyFilter & bit;
            if (ImGui::RadioButton(name, isSet)) {
                hierarchyFilter = isSet ? hierarchyFilter & ~bit : hierarchyFilter | bit;
            }
            };

        if (ImGui::BeginPopup("Hierarchy filter settings")) {
            ImGui::Text("Component filter settings");
            ImGui::Separator();

            auto prevHierarchyFilter = hierarchyFilter;
            HierarchyFilter filtered = hierarchyFilter & HierarchyFilterBits::AllBit;
            bool isSet = filtered == HierarchyFilterBits::AllBit;

            if (ImGui::RadioButton("All", isSet)) {
                hierarchyFilter = isSet ? 0 : HierarchyFilterBits::AllBit;
            }
            renderSingleBitOption("Name component", HierarchyFilterBits::NameBit);
            renderSingleBitOption("Transform component", HierarchyFilterBits::TransformBit);
            renderSingleBitOption("Mesh component", HierarchyFilterBits::MeshBit);
            renderSingleBitOption("Text component", HierarchyFilterBits::TextBit);
            renderSingleBitOption("Audio component", HierarchyFilterBits::AudioBit);
            renderSingleBitOption("Audio volume component", HierarchyFilterBits::AudioVolumeBit);
            renderSingleBitOption("Player component", HierarchyFilterBits::PlayerBit);
            renderSingleBitOption("Rigid body component", HierarchyFilterBits::RigidBodyBit);
            renderSingleBitOption("Camera component", HierarchyFilterBits::CameraBit);
            renderSingleBitOption("Script component", HierarchyFilterBits::ScriptBit);
            renderSingleBitOption("Light component", HierarchyFilterBits::LightBit);

            hierarchyFilterChanged = prevHierarchyFilter != hierarchyFilter;

            ImGui::EndPopup();
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

    bool SceneHierarchyPanel::SearchHierarchy(EntityData& data, ECS::Entity entity,
        std::unordered_set<ECS::Entity>& matchSet, std::string& nodeName, bool parentMatches) {

        auto hierarchyComponent = data.hierarchyPool.TryGet(entity);
        auto nameComponent = data.namePool.TryGet(entity);

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
        parentMatches |= transformedEntitySearch.empty();

        if (hierarchyFilter != HierarchyFilterBits::AllBit) {
            auto checkBit = [&]<class T>(ECS::Pool<T>&pool, HierarchyFilterBits bit) -> bool {
                if (hierarchyFilter & bit) {
                    return pool.Contains(entity);
                }
                return false;
            };

            bool bitChecks = false;
            bitChecks |= checkBit(data.namePool, HierarchyFilterBits::NameBit);
            bitChecks |= checkBit(data.transformPool, HierarchyFilterBits::TransformBit);
            bitChecks |= checkBit(data.meshPool, HierarchyFilterBits::MeshBit);
            bitChecks |= checkBit(data.textPool, HierarchyFilterBits::TextBit);
            bitChecks |= checkBit(data.audioPool, HierarchyFilterBits::AudioBit);
            bitChecks |= checkBit(data.audioVolumePool, HierarchyFilterBits::AudioVolumeBit);
            bitChecks |= checkBit(data.playerPool, HierarchyFilterBits::PlayerBit);
            bitChecks |= checkBit(data.rigidBodyPool, HierarchyFilterBits::RigidBodyBit);
            bitChecks |= checkBit(data.cameraPool, HierarchyFilterBits::CameraBit);
            bitChecks |= checkBit(data.scriptPool, HierarchyFilterBits::ScriptBit);
            bitChecks |= checkBit(data.lightPool, HierarchyFilterBits::LightBit);

            parentMatches &= bitChecks;
        }

        bool matches = parentMatches;

        if (hierarchyComponent) {

            auto children = hierarchyComponent->GetChildren();
            for (auto childEntity : children) {

                matches |= SearchHierarchy(data, childEntity, matchSet, nodeName, parentMatches);

            }

        }

        if (matches)
            matchSet.insert(entity);

        return matches;

    }

    void SceneHierarchyPanel::ToggleHierarchyVisibility(Scene::Entity entity, bool visible) {

        auto meshComponent = entity.TryGetComponent<MeshComponent>();
        if (meshComponent) {
            meshComponent->visible = visible;
        }

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        if (!hierarchyComponent)
            return;

        for (auto child : hierarchyComponent->GetChildren())
            ToggleHierarchyVisibility(child, visible);
        

    }

}