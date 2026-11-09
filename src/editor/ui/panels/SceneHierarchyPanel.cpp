#include "SceneHierarchyPanel.h"

#include "../Widgets.h"

#include "Clock.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <ImGuizmo.h>

namespace Atlas::Editor::UI {

    void SceneHierarchyPanel::Update(Ref<Scene::Scene>& scene) {

        if (Singletons::blockingOperation->block)
            return;

        if (!searchJob.HasFinished())
            return;

        if (!searchJobResultRetrieved) {
            std::swap(matchSet, newMatchSet);
            matchAll = false;
            openSearchMatches |= openSearchMatchesPending;
            openSearchMatchesPending = false;
            searchJobResultRetrieved = true;
        }

        bool searchChanged = entitySearch != lastEntitySearch;

        // Without a search or filter everything matches, large scenes don't need to go through all their entities
        if (entitySearch.empty() && (hierarchyFilter & HierarchyFilterBits::AllBit) == HierarchyFilterBits::AllBit) {
            if (!matchAll) {
                matchAll = true;
                std::unordered_set<ECS::Entity>().swap(matchSet);
                std::unordered_set<ECS::Entity>().swap(newMatchSet);
            }
            openSearchMatches |= searchChanged;
            lastEntitySearch = entitySearch;
            hierarchyFilterChanged = false;
            return;
        }

        bool settingsChanged = searchChanged || hierarchyFilterChanged || matchAll;
        if (!settingsChanged && lastAliveEntityCount == scene->GetEntityCount())
            return;

        // Entities created while playing shouldn't copy all the pools every frame
        if (!settingsChanged && Clock::Get() - lastSearchTime < 1.0f)
            return;

        openSearchMatchesPending |= searchChanged;
        lastSearchTime = Clock::Get();
        lastEntitySearch = entitySearch;
        lastAliveEntityCount = scene->GetEntityCount();
        hierarchyFilterChanged = false;
        searchJobResultRetrieved = false;

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
            .fogVolumePool = scene->entityManager.GetPool<FogVolumeComponent>(),
            .search = entitySearch,
            .filter = hierarchyFilter,
        };

        // Search should be case-insensitive
        std::transform(data.search.begin(), data.search.end(), data.search.begin(), ::tolower);

        auto root = scene->GetRootEntity();
        // Aquire scene here such that it can't be freed. The job only works on copies, the scene and UI can change meanwhile
        JobSystem::Execute(searchJob, [&, scene = scene, root = root, data = std::move(data)](JobData&) mutable {

            newMatchSet.clear();
            std::string nodeName;
            SearchHierarchy(data, root, newMatchSet, nodeName, false);
            });

    }

    void SceneHierarchyPanel::Render(Ref<Scene::Scene>& scene, bool inFocus) {

        const float padding = 8.0f;

        ImGui::Begin(GetNameID());

        if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
            ImGui::SetWindowFocus();

        isFocused = ImGui::IsWindowFocused();

        if (scene != nullptr) {

            bool selectionChanged = false;

            hierarchyPool = &scene->entityManager.GetPool<HierarchyComponent>();

            // Finding the root goes through all hierarchies
            Scene::Entity root(rootEntity, &scene->entityManager);
            auto rootHierarchy = root.IsValid() ? hierarchyPool->TryGet(root) : nullptr;
            if (!rootHierarchy || !rootHierarchy->root) {
                root = scene->GetRootEntity();
                rootEntity = root;
            }

            if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
                // Unselect entity in this case, since this popup could only be opened by clicking on emtpy space
                ClearSelection();

                ImGui::SeparatorText("Create");
                RenderCreateMenuItems(root);

                ImGui::EndPopup();
            }

            auto region = ImGui::GetContentRegionAvail();
            auto lineHeight = ImGui::GetTextLineHeight();
            auto buttonSize = ImVec2(lineHeight, lineHeight);

            // Wrap input text field into child, such that we can adjust the total size to not overlap anything
            // Leave exactly enough room for the filter image button (icon + frame padding) next to it
            const auto& style = ImGui::GetStyle();
            auto filterButtonWidth = buttonSize.x + 2.0f * style.FramePadding.x;
            auto createButtonWidth = ImGui::GetFrameHeight();
            ImGui::BeginChild("Search field", ImVec2(region.x - filterButtonWidth - createButtonWidth - 2.0f * style.ItemSpacing.x,
                ImGui::GetFrameHeight()));
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##Search", "Search entities...", &entitySearch);
            if (ImGui::IsItemClicked()) selectionChanged = true;
            ImGui::EndChild();

            ImGui::SameLine();
            if (ImGui::Button("+", ImVec2(createButtonWidth, 0.0f)))
                ImGui::OpenPopup("Create entity");
            ImGui::SetItemTooltip("Create entity");
            if (ImGui::BeginPopup("Create entity")) {
                ImGui::SeparatorText("Create");
                RenderCreateMenuItems(root);
                ImGui::EndPopup();
            }

            RenderFilterPopup();

            // A hidden window doesn't submit anything
            if (!ImGui::GetCurrentWindow()->SkipItems) {
                TraverseHierarchy(scene, root, &selectionChanged);
                FlushSkippedRows();

                openSearchMatches = false;
                // Entities hidden by the search can't be revealed
                revealEntity = ECS::EntityConfig::InvalidEntity;
                revealOpenSet.clear();
            }

            if (showSceneSettings)
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
                DeleteSelectedEntities(scene);
            if (inFocus && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2, false) && selectedEntity.IsValid())
                StartRename(selectedEntity);

            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !selectionChanged) {
                ClearSelection();
            }

            // Need to get rid of this set once the mouse button is released
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                toggledOpenSet.clear();
            }

        }

        ImGui::End();

    }

    void SceneHierarchyPanel::SelectEntity(Scene::Entity entity, bool keepSelection) {

        if (!keepSelection)
            ClearSelection();

        // Toggle selection if entity is already there
        if (selectedEntities.contains(entity)) {
            if (selectedEntity == entity)
                selectedEntity = Scene::Entity();
            selectedEntities.erase(entity);
            return;
        }

        selectedEntity = entity;
        selectedEntities.insert(entity);

    }

    bool SceneHierarchyPanel::RenderCreateMenuItems(Scene::Entity parent) {

        bool created = false;
        auto item = [&](const char* name, EntityTemplate type) {
            if (ImGui::MenuItem(name)) {
                createRequest = { .type = type, .parent = parent, .pending = true };
                created = true;
            }
            };

        item("Empty entity", EntityTemplate::Empty);
        ImGui::Separator();
        item("Point light", EntityTemplate::PointLight);
        item("Spot light", EntityTemplate::SpotLight);
        item("Directional light", EntityTemplate::DirectionalLight);
        ImGui::Separator();
        item("Camera", EntityTemplate::Camera);
        item("Audio source", EntityTemplate::AudioSource);
        item("Text", EntityTemplate::Text);
        ImGui::Separator();
        item("Fog volume", EntityTemplate::FogVolume);

        return created;

    }

    void SceneHierarchyPanel::StartRename(Scene::Entity entity) {

        auto nameComponent = entity.TryGetComponent<NameComponent>();
        // The root entity is found by its name, so it must not be renamed
        if (!nameComponent || nameComponent->name == "Root")
            return;

        renamingEntity = entity;
        renameBuffer = nameComponent->name;
        renameFocusPending = true;
        renameFrameCount = 0;

    }

    void SceneHierarchyPanel::RevealEntity(const Ref<Scene::Scene>& scene, Scene::Entity entity) {

        revealEntity = entity;
        revealOpenSet.clear();
        for (auto parent = scene->GetParentEntity(entity); parent.IsValid(); parent = scene->GetParentEntity(parent))
            revealOpenSet.insert(parent);

    }

    void SceneHierarchyPanel::ClearSelection() {

        selectedEntity = Scene::Entity();
        selectedEntities.clear();
        selectedProperty = SelectedProperty();
        selectedType = EntitySelectionType::Entity;

    }

    void SceneHierarchyPanel::TraverseHierarchy(Ref<Scene::Scene>& scene, Scene::Entity entity, bool* selectionChanged) {

        // If we have a search term and the name doesn't match, return
        bool matches = matchAll || matchSet.contains(entity);
        if (!entity.IsValid() || (entity != rootEntity && !matches))
            return;

        auto window = ImGui::GetCurrentWindow();
        auto hierarchyComponent = hierarchyPool->TryGet(entity);

        // Same id and state as the tree node of the entity
        auto storage = ImGui::GetStateStorage();
        ImGuiID id = 0;
        if (hierarchyComponent) {
            id = window->GetID(reinterpret_cast<void*>(static_cast<uint64_t>(entity)));
            if (openSearchMatches)
                storage->SetInt(id, !entitySearch.empty() && matches ? 1 : 0);
            if (!revealOpenSet.empty() && revealOpenSet.contains(entity))
                storage->SetInt(id, 1);
        }

        // Rows outside of the window only count for the size of the content. A few more than visible are
        // submitted, such that keyboard navigation can leave the visible area
        auto rowY = window->DC.CursorPos.y + float(skippedRows) * rowHeight;
        auto margin = 2.0f * rowHeight;
        bool isRowVisible = rowY + rowHeight >= window->ClipRect.Min.y - margin && rowY <= window->ClipRect.Max.y + margin;

        if (!isRowVisible && entity != revealEntity) {
            skippedRows++;

            if (hierarchyComponent && storage->GetInt(id, 0) != 0) {
                ImGui::TreePushOverrideID(id);
                TraverseChildren(scene, entity, selectionChanged);
                ImGui::TreePop();
            }
            return;
        }

        FlushSkippedRows();
        RenderEntityRow(scene, entity, selectionChanged);

    }

    void SceneHierarchyPanel::TraverseChildren(Ref<Scene::Scene>& scene, Scene::Entity entity, bool* selectionChanged) {

        auto hierarchyComponent = hierarchyPool->TryGet(entity);
        if (!hierarchyComponent)
            return;

        auto children = &hierarchyComponent->GetChildren();
        for (size_t i = 0; i < children->size(); i++) {
            auto lastRenderedRows = renderedRows;

            TraverseHierarchy(scene, (*children)[i], selectionChanged);

            // Only rendered rows can change the hierarchy, e.g. by deleting entities
            if (renderedRows != lastRenderedRows) {
                hierarchyComponent = hierarchyPool->TryGet(entity);
                if (!hierarchyComponent)
                    return;
                children = &hierarchyComponent->GetChildren();
            }
        }

    }

    void SceneHierarchyPanel::FlushSkippedRows() {

        if (skippedRows > 0)
            ImGui::Dummy(ImVec2(0.0f, std::max(float(skippedRows) * rowHeight - ImGui::GetStyle().ItemSpacing.y, 0.0f)));
        skippedRows = 0;

    }

    void SceneHierarchyPanel::RenderEntityRow(Ref<Scene::Scene>& scene, Scene::Entity entity, bool* selectionChanged) {

        renderedRows++;
        auto rowStartY = ImGui::GetCurrentWindow()->DC.CursorPos.y;

        ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        auto nameComponent = entity.TryGetComponent<NameComponent>();

        if (!hierarchyComponent) {
            baseFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        std::string nodeName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

        bool rootNode = nodeName == "Root";

        auto nodeFlags = baseFlags;
        nodeFlags |= selectedEntities.contains(entity) ? ImGuiTreeNodeFlags_Selected : 0;

        bool nodeOpen = false;
        bool createEntity = false;
        bool deleteEntity = false;
        bool duplicateEntity = false;
        bool droppedEntities = false;

        auto entityId = static_cast<uint64_t>(entity);

        // The visibility button sits in a fixed column on the right side of the row. The name is
        // clipped before it, such that long names don't run underneath the button
        const float eyePadding = 2.0f;
        auto eyeIconSize = ImGui::GetTextLineHeight();
        auto eyeExtent = eyeIconSize + 2.0f * eyePadding;
        auto eyeMinX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - eyeExtent;

        // Hidden entities are dimmed
        bool isHidden = nodeInvisibleSet.contains(entity);
        if (isHidden)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        bool isRenaming = renamingEntity == entity && nameComponent && !rootNode;
        nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(entityId), nodeFlags, "");

        auto rowMin = ImGui::GetItemRectMin();
        auto rowMax = ImGui::GetItemRectMax();

        bool nameTruncated = false;
        if (!isRenaming) {
            auto textPos = ImVec2(rowMin.x + ImGui::GetTreeNodeToLabelSpacing(), rowMin.y + ImGui::GetStyle().FramePadding.y);
            auto textMaxX = eyeMinX - ImGui::GetStyle().ItemInnerSpacing.x;
            nameTruncated = textPos.x + ImGui::CalcTextSize(nodeName.c_str()).x > textMaxX;
            ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), textPos, ImVec2(textMaxX, rowMax.y),
                textMaxX, textMaxX, nodeName.c_str(), nullptr, nullptr);
        }
        if (isHidden)
            ImGui::PopStyleColor();

        // Full name for clipped names
        if (nameTruncated && !ImGui::IsDragDropActive())
            ImGui::SetItemTooltip("%s", nodeName.c_str());

        // Double click focuses the camera on the entity
        if (!isRenaming && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsItemToggledOpen())
            focusRequested = true;

        bool rowHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);

        if (ImGui::IsItemToggledOpen())
            toggledOpenSet.insert(entity);

        bool shiftPressed = ImGui::IsKeyDown(ImGuiKey_LeftShift);
        if (!shiftPressed && (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !toggledOpenSet.contains(entity))) {
            SelectEntity(entity);
        }
        if (shiftPressed && (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !toggledOpenSet.contains(entity))) {
            SelectEntity(entity, true);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !toggledOpenSet.contains(entity) && !selectedEntities.contains(entity)) {
            SelectEntity(entity);
        }

        // In any case we need to notify that we clicked on an item
        if ((ImGui::IsItemClicked(ImGuiMouseButton_Right) || ImGui::IsItemClicked(ImGuiMouseButton_Left)) && !ImGui::IsItemToggledOpen()) {
            *selectionChanged = true;
        }

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(typeid(Scene::Entity).name(), &entity, sizeof(Scene::Entity));
            // Show what is dragged, the hint is secondary
            if (selectedEntities.size() > 1)
                ImGui::Text("%d entities", int32_t(selectedEntities.size()));
            else
                ImGui::TextUnformatted(nodeName.c_str());
            ImGui::TextDisabled("Drop on an entity to reparent, or into the content browser to create a prefab");

            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            auto dropPayload = ImGui::GetDragDropPayload();
            if (dropPayload->IsDataType(typeid(Scene::Entity).name())) {
                droppedEntities = ImGui::AcceptDragDropPayload(typeid(Scene::Entity).name());
            }

            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Focus", "F"))
                focusRequested = true;

            if (!rootNode && nameComponent && ImGui::MenuItem("Rename", "F2"))
                StartRename(entity);

            ImGui::Separator();

            if (ImGui::BeginMenu("Create child")) {
                createEntity = RenderCreateMenuItems(entity);
                ImGui::EndMenu();
            }

            if (!rootNode && ImGui::MenuItem("Duplicate entity"))
                duplicateEntity = true;

            // We shouldn't allow the user to delete the root entity
            if (!rootNode && ImGui::MenuItem("Delete entity"))
                deleteEntity = true;

            ImGui::EndPopup();
        }

        if (isRenaming) {
            const auto& style = ImGui::GetStyle();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(eyeMinX - ImGui::GetCursorScreenPos().x - style.ItemInnerSpacing.x);
            if (renameFocusPending) {
                ImGui::SetKeyboardFocusHere();
                renameFocusPending = false;
            }

            bool confirmed = ImGui::InputText("##Rename", &renameBuffer,
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            bool cancelled = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
            // Clicking somewhere else confirms as well, escape cancels
            bool finished = confirmed || ImGui::IsItemDeactivated() ||
                (!ImGui::IsItemActive() && renameFrameCount > 2);
            renameFrameCount++;

            if (finished) {
                if (!cancelled && !renameBuffer.empty() && renameBuffer != nameComponent->name) {
                    if (history)
                        history->BeginEdit(scene, { entity });
                    nameComponent->name = renameBuffer;
                    if (history)
                        history->EndEdit("Rename " + renameBuffer);
                    sceneChanged = true;
                }
                renamingEntity = Scene::Entity();
            }
        }

        ImGui::SameLine();

        bool isNodeVisible = !isHidden;
        auto& visibilityIcon = Singletons::icons->Get(isNodeVisible ? IconType::Visibility : IconType::VisibilityOff);
        auto set = Singletons::imguiWrapper->GetTextureId(&visibilityIcon);

        // Keep the eye subtle unless the row is hovered or the entity is hidden
        auto iconAlpha = rowHovered || !isNodeVisible ? 1.0f : 0.4f;

        ImGui::SetCursorScreenPos(ImVec2(eyeMinX, rowMin.y + (rowMax.y - rowMin.y - eyeExtent) * 0.5f));

        ImGui::PushID(reinterpret_cast<void*>(entityId));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(eyePadding, eyePadding));

        if (ImGui::ImageButton("Visibility button", set, ImVec2(eyeIconSize, eyeIconSize), ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f),
            ImVec4(0.0f, 0.0f, 0.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, iconAlpha))) {
            isNodeVisible = !isNodeVisible;

            std::vector<Scene::Entity> meshEntities;
            CollectMeshEntities(entity, meshEntities);
            if (history)
                history->BeginEdit(scene, meshEntities);
            ToggleHierarchyVisibility(entity, isNodeVisible);
            if (history)
                history->EndEdit(isNodeVisible ? "Show " + nodeName : "Hide " + nodeName);
            sceneChanged = true;

            // We are checking the new state here
            if (!isNodeVisible)
                nodeInvisibleSet.insert(entity);
            else
                nodeInvisibleSet.erase(entity);
        }

        ImGui::SetItemTooltip(isNodeVisible ? "Hide" : "Show");

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::PopID();

        // Skipped rows move by the same distance
        rowHeight = ImGui::GetCurrentWindow()->DC.CursorPos.y - rowStartY;

        if (entity == revealEntity) {
            ImGui::SetScrollHereY(0.5f);
            revealEntity = ECS::EntityConfig::InvalidEntity;
        }

        if (nodeOpen && hierarchyComponent) {

            TraverseChildren(scene, entity, selectionChanged);

            ImGui::TreePop();

        }

        if (rootNode) {
            duplicateEntity = false;
            deleteEntity = false;
        }

        if (deleteEntity) {
            DeleteSelectedEntities(scene);
        }

        if (duplicateEntity) {
            DuplicateSelectedEntity(scene);
        }

        *selectionChanged |= createEntity | deleteEntity | duplicateEntity;

        if (droppedEntities) {
            std::vector<Scene::Entity> movedEntities;
            for (auto ecsEntity : selectedEntities) {
                Scene::Entity dropEntity(ecsEntity, &scene->entityManager);
                if (entity != dropEntity)
                    movedEntities.push_back(dropEntity);
            }

            if (history)
                history->BeginMove(scene, movedEntities);
            for (auto dropEntity : movedEntities) {
                dropEntity.SetParent(entity);
                sceneChanged = true;
            }
            if (history)
                history->EndMove("Move entities");

        }

        // If the hierarchy is emtpy after movements or deletions, also remove the hierarchy
        hierarchyComponent = deleteEntity ? nullptr : entity.TryGetComponent<HierarchyComponent>();
        if (hierarchyComponent && hierarchyComponent->GetChildren().empty()) {
            entity.RemoveComponent<HierarchyComponent>();
        }

    }

    void SceneHierarchyPanel::RenderExtendedHierarchy(const Ref<Scene::Scene>& scene, bool* selectionChanged) {

        ImGui::Spacing();
        ImGui::SeparatorText("Scene settings");

        RenderExtendedItem("Terrain", IconType::Terrain, &selectedProperty.terrain, selectionChanged);
        RenderExtendedItem("Ocean", IconType::Ocean, &selectedProperty.ocean, selectionChanged);
        RenderExtendedItem("Navigation", IconType::Navigation, &selectedProperty.navigation, selectionChanged);
        if (scene->irradianceVolume)
            RenderExtendedItem("Irradiance volume", IconType::IrradianceVolume, &selectedProperty.irradianceVolume, selectionChanged);
        if (scene->rtgi)
            RenderExtendedItem("Raytraced global illumination", IconType::GlobalIllumination, &selectedProperty.rtgi, selectionChanged);
        if (scene->ssgi)
            RenderExtendedItem("Screen-space global illumination", IconType::GlobalIllumination, &selectedProperty.ssgi, selectionChanged);
        if (scene->reflection)
            RenderExtendedItem("Reflection", IconType::Reflection, &selectedProperty.reflection, selectionChanged);
        if (scene->sss)
            RenderExtendedItem("Screen-space shadows", IconType::Shadow, &selectedProperty.sss, selectionChanged);
        if (scene->sky.clouds)
            RenderExtendedItem("Volumetric clouds", IconType::Clouds, &selectedProperty.volumetricClouds, selectionChanged);
        if (scene->fog)
            RenderExtendedItem("Fog", IconType::Fog, &selectedProperty.fog, selectionChanged);
        RenderExtendedItem("Sky", IconType::Sky, &selectedProperty.sky, selectionChanged);
        RenderExtendedItem("Wind", IconType::Wind, &selectedProperty.wind, selectionChanged);
        RenderExtendedItem("Post processing", IconType::PostProcessing, &selectedProperty.postProcessing, selectionChanged);

    }

    void SceneHierarchyPanel::RenderExtendedItem(const std::string& name, IconType iconType, bool* selected, bool* selectionChanged) {

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

        nodeFlags |= *selected ? ImGuiTreeNodeFlags_Selected : 0;
        ImGui::TreeNodeEx(name.c_str(), nodeFlags);

        // A leaf has no arrow, the icon takes its place
        auto iconSize = ImGui::GetFontSize();
        auto iconPos = ImGui::GetItemRectMin();
        iconPos.x += ImGui::GetStyle().FramePadding.x;
        iconPos.y += ImGui::GetStyle().FramePadding.y;
        auto set = Singletons::imguiWrapper->GetTextureId(&Singletons::icons->Get(iconType));
        ImGui::GetWindowDrawList()->AddImage(set, iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
            ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.75f)));

        if (ImGui::IsItemClicked()) {
            ClearSelection();
            *selected = true;
            *selectionChanged = true;
        }

    }

    void SceneHierarchyPanel::RenderFilterPopup() {

        ImGui::SameLine();

        auto region = ImGui::GetContentRegionAvail();
        auto lineHeight = ImGui::GetTextLineHeight();
        auto buttonSize = ImVec2(lineHeight, lineHeight);

        auto& filterIcon = Singletons::icons->Get(IconType::Filter);
        auto set = Singletons::imguiWrapper->GetTextureId(&filterIcon);

        auto uvMin = ImVec2(0.1f, 0.1f);
        auto uvMax = ImVec2(0.9f, 0.9f);

        //ImGui::SetCursorPosX(ImVec2(region.x - (buttonSize.x + 2.0f * padding), ImGui::GetCursorPosY()));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        if (ImGui::ImageButton("Filter button", set, buttonSize, uvMin, uvMax)) {
            ImGui::OpenPopup("Hierarchy filter settings");
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // Mark the button while a filter hides entities
        bool filterActive = (hierarchyFilter & HierarchyFilterBits::AllBit) != HierarchyFilterBits::AllBit;
        ImGui::SetItemTooltip(filterActive ? "Filter entities (active)" : "Filter entities");
        if (filterActive)
            Widgets::ActiveIndicator();

        // Filters can be combined, so they are checkboxes
        auto renderSingleBitOption = [&](const char* name, HierarchyFilterBits bit) {
            bool isSet = hierarchyFilter & bit;
            if (ImGui::Checkbox(name, &isSet)) {
                hierarchyFilter = isSet ? hierarchyFilter | bit : hierarchyFilter & ~bit;
            }
            };

        if (ImGui::BeginPopup("Hierarchy filter settings")) {
            ImGui::SeparatorText("Show entities with");

            auto prevHierarchyFilter = hierarchyFilter;
            HierarchyFilter filtered = hierarchyFilter & HierarchyFilterBits::AllBit;
            bool isSet = filtered == HierarchyFilterBits::AllBit;

            if (ImGui::Checkbox("All", &isSet)) {
                hierarchyFilter = isSet ? HierarchyFilterBits::AllBit : 0;
            }
            ImGui::Separator();
            renderSingleBitOption("Name", HierarchyFilterBits::NameBit);
            renderSingleBitOption("Transform", HierarchyFilterBits::TransformBit);
            renderSingleBitOption("Mesh", HierarchyFilterBits::MeshBit);
            renderSingleBitOption("Text", HierarchyFilterBits::TextBit);
            renderSingleBitOption("Audio", HierarchyFilterBits::AudioBit);
            renderSingleBitOption("Audio volume", HierarchyFilterBits::AudioVolumeBit);
            renderSingleBitOption("Player", HierarchyFilterBits::PlayerBit);
            renderSingleBitOption("Rigid body", HierarchyFilterBits::RigidBodyBit);
            renderSingleBitOption("Camera", HierarchyFilterBits::CameraBit);
            renderSingleBitOption("Script", HierarchyFilterBits::ScriptBit);
            renderSingleBitOption("Light", HierarchyFilterBits::LightBit);
            renderSingleBitOption("Fog volume", HierarchyFilterBits::FogVolumeBit);

            hierarchyFilterChanged = prevHierarchyFilter != hierarchyFilter;

            ImGui::EndPopup();
        }

    }

    void SceneHierarchyPanel::DeleteSelectedEntities(Ref<Scene::Scene>& scene) {

        sceneChanged |= !selectedEntities.empty();

        std::vector<Scene::Entity> deletedEntities;
        for (auto ecsEntity : selectedEntities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (entity.IsValid())
                deletedEntities.push_back(entity);
        }

        if (history)
            history->BeginChange(scene, deletedEntities);
        // Descendants of deleted entities are already gone
        for (auto entity : deletedEntities)
            if (entity.IsValid())
                scene->DestroyEntity(entity);
        if (history)
            history->EndChange(deletedEntities.size() == 1 ? "Delete entity" : "Delete entities", {});

        ClearSelection();

    }

    void SceneHierarchyPanel::DuplicateSelectedEntity(Ref<Scene::Scene>& scene) {

        sceneChanged |= !selectedEntities.empty();

        auto oldSelectedEntites = selectedEntities;

        ClearSelection();

        if (history)
            history->BeginChange(scene, {});

        for (auto ecsEntity : oldSelectedEntites) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (!entity.IsValid())
                continue;

            auto newEntity = scene->DuplicateEntity(entity);

            // The hierarchy only shows entities below the root
            if (!scene->GetParentEntity(newEntity).IsValid())
                newEntity.SetParent(scene->GetRootEntity());

            SelectEntity(newEntity, true);
        }

        if (history) {
            std::vector<Scene::Entity> newEntities;
            for (auto ecsEntity : selectedEntities)
                newEntities.emplace_back(ecsEntity, &scene->entityManager);
            history->EndChange(newEntities.size() == 1 ? "Duplicate entity" : "Duplicate entities", newEntities);
        }

    }

    bool SceneHierarchyPanel::SearchHierarchy(EntityData& data, ECS::Entity entity,
        std::unordered_set<ECS::Entity>& matchSet, std::string& nodeName, bool parentMatches) {

        auto hierarchyComponent = data.hierarchyPool.TryGet(entity);
        auto nameComponent = data.namePool.TryGet(entity);

        // Only allocate a new string when there is no name component
        if (!nameComponent)
            nodeName = "entity " + std::to_string(entity);
        const auto& name = nameComponent ? nameComponent->name : nodeName;

        auto it = std::search(name.begin(), name.end(), data.search.begin(), data.search.end(),
            [](unsigned char nodeChar, unsigned char searchChar) { return std::tolower(nodeChar) == searchChar; });

        parentMatches |= it != name.end();
        parentMatches |= data.search.empty();

        if (data.filter != HierarchyFilterBits::AllBit) {
            auto checkBit = [&](const ECS::Storage& storage, HierarchyFilterBits bit) -> bool {
                if (data.filter & bit) {
                    return storage.Contains(entity);
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
            bitChecks |= checkBit(data.fogVolumePool, HierarchyFilterBits::FogVolumeBit);

            parentMatches &= bitChecks;
        }

        bool matches = parentMatches;

        if (hierarchyComponent) {

            const auto& children = hierarchyComponent->GetChildren();
            for (auto childEntity : children) {

                matches |= SearchHierarchy(data, childEntity, matchSet, nodeName, parentMatches);

            }

        }

        if (matches)
            matchSet.insert(entity);

        return matches;

    }

    void SceneHierarchyPanel::CollectMeshEntities(Scene::Entity entity, std::vector<Scene::Entity>& entities) {

        if (entity.HasComponent<MeshComponent>())
            entities.push_back(entity);

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        if (!hierarchyComponent)
            return;

        for (auto child : hierarchyComponent->GetChildren())
            CollectMeshEntities(child, entities);

    }

    void SceneHierarchyPanel::ToggleHierarchyVisibility(Scene::Entity entity, bool visible) {

        auto meshComponent = entity.TryGetComponent<MeshComponent>();
        if (meshComponent) {
            meshComponent->SetVisible(visible);
        }

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        if (!hierarchyComponent)
            return;

        for (auto child : hierarchyComponent->GetChildren())
            ToggleHierarchyVisibility(child, visible);


    }

}
