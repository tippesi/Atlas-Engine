#include "SceneHierarchyPanel.h"
#include "scene/SceneSerializer.h"

#include "../Widgets.h"
#include "../../tools/AssetPlacer.h"
#include "../../tools/ResourcePayloadHelper.h"

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
        if (entitySearch.empty() && (hierarchyFilter & GetAllFilterBits()) == GetAllFilterBits()) {
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
            .componentStorages = Scene::CopyComponentStorages(scene.get()),
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

    void SceneHierarchyPanel::Render(Ref<Scene::Scene>& scene) {

        const float padding = 8.0f;

        ImGui::Begin(GetNameID());

        if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered() && !ImGui::IsWindowFocused())
            ImGui::SetWindowFocus();

        // The sticky rows are a child window
        isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);

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
                openRows.clear();
                stickyRows.clear();
                stickyRowsFound = false;

                TraverseHierarchy(scene, root, &selectionChanged);
                FlushSkippedRows();
                RenderStickyRows(scene, &selectionChanged);

                openSearchMatches = false;
                // Entities hidden by the search can't be revealed
                revealEntity = ECS::EntityConfig::InvalidEntity;
                revealOpenSet.clear();
            }

            if (showSceneSettings)
                RenderExtendedHierarchy(scene, &selectionChanged);

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
        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        if (!nameComponent || (hierarchyComponent && hierarchyComponent->root))
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

        // The first row below the stack of scrolled out ancestors decides which of them stay at the top
        if (!stickyRowsFound) {
            auto top = window->ClipRect.Min.y;
            // At most half of the window, the outermost ancestors stay
            auto maxCount = size_t(std::max(1.0f, 0.5f * window->ClipRect.GetHeight() / rowHeight));
            size_t count = 0;
            while (count < openRows.size() && count < maxCount && openRows[count].position.y < top + float(count) * rowHeight)
                count++;
            if (rowY >= top + float(count) * rowHeight) {
                stickyRows.assign(openRows.begin(), openRows.begin() + count);
                stickyRowsFound = true;
            }
        }

        if (!isRowVisible && entity != revealEntity) {
            skippedRows++;

            if (hierarchyComponent && storage->GetInt(id, 0) != 0) {
                openRows.push_back({ entity, id, ImVec2(window->DC.CursorPos.x, rowY) });
                ImGui::TreePushOverrideID(id);
                TraverseChildren(scene, entity, selectionChanged);
                ImGui::TreePop();
                openRows.pop_back();
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

    void SceneHierarchyPanel::RenderStickyRows(Ref<Scene::Scene>& scene, bool* selectionChanged) {

        if (stickyRows.empty())
            return;

        auto window = ImGui::GetCurrentWindow();
        auto storage = ImGui::GetStateStorage();
        auto cursor = ImGui::GetCursorScreenPos();
        auto top = window->ClipRect.Min.y;
        auto height = float(stickyRows.size()) * rowHeight;

        // A child window on top of the tree, such that the rows below don't get its clicks
        ImGui::SetCursorScreenPos(ImVec2(window->ClipRect.Min.x, top));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("Sticky rows", ImVec2(window->ClipRect.GetWidth(), height), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        for (size_t i = 0; i < stickyRows.size(); i++) {
            const auto& row = stickyRows[i];
            auto y = top + float(i) * rowHeight;
            // The rows of the tree might have deleted it
            if (!row.entity.IsValid())
                continue;
            auto nameComponent = row.entity.TryGetComponent<NameComponent>();
            auto name = nameComponent ? nameComponent->name : "Entity " + std::to_string(row.entity);

            ImGui::SetCursorScreenPos(ImVec2(row.position.x, y));
            ImGui::SetNextItemOpen(true);
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                ImGuiTreeNodeFlags_NoTreePushOnOpen;
            flags |= selectedEntities.contains(row.entity) ? ImGuiTreeNodeFlags_Selected : 0;
            bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(row.entity)), flags, "%s", name.c_str());

            if (!open) {
                // Collapsing scrolls the tree such that the entity stays where its header was
                storage->SetInt(row.id, 0);
                ImGui::SetScrollY(window, window->Scroll.y + row.position.y - y);
            }
            else if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
                SelectEntity(row.entity, ImGui::IsKeyDown(ImGuiKey_LeftShift));
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    focusRequested = true;
                *selectionChanged = true;
            }

            if (auto droppedEntity = AcceptDrop(scene, row.entity); droppedEntity.IsValid())
                MoveEntities(scene, row.entity, droppedEntity);
        }

        ImGui::GetWindowDrawList()->AddLine(ImVec2(window->ClipRect.Min.x, top + height - 1.0f),
            ImVec2(window->ClipRect.Max.x, top + height - 1.0f), ImGui::GetColorU32(ImGuiCol_Separator));

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::SetCursorScreenPos(cursor);

    }

    Scene::Entity SceneHierarchyPanel::AcceptDrop(const Ref<Scene::Scene>& scene, Scene::Entity entity) {

        Scene::Entity droppedEntity;
        if (!ImGui::BeginDragDropTarget())
            return droppedEntity;

        auto dropPayload = ImGui::GetDragDropPayload();
        if (dropPayload->IsDataType(typeid(Scene::Entity).name())) {
            // Entities dragged from another scene window stay where they are
            auto payload = ImGui::AcceptDragDropPayload(typeid(Scene::Entity).name());
            if (payload && static_cast<Scene::Entity*>(payload->Data)->GetScene() == scene.get())
                droppedEntity = *static_cast<Scene::Entity*>(payload->Data);
        }
        else if (auto path = ResourcePayloadHelper::GetPath(); AssetPlacer::CanPlace(path) &&
            ImGui::AcceptDragDropPayload("ContentBrowserResource")) {
            assetDrop = { .path = path, .parent = entity };
        }

        ImGui::EndDragDropTarget();
        return droppedEntity;

    }

    void SceneHierarchyPanel::MoveEntities(Ref<Scene::Scene>& scene, Scene::Entity parent, Scene::Entity droppedEntity) {

        // The selection moves when the dragged entity is part of it, ancestors of the target can't move below it
        std::vector<Scene::Entity> movedEntities;
        if (selectedEntities.contains(droppedEntity)) {
            for (auto ecsEntity : selectedEntities)
                movedEntities.emplace_back(ecsEntity, &scene->entityManager);
        }
        else {
            movedEntities.push_back(droppedEntity);
        }
        for (auto ancestor = parent; ancestor.IsValid(); ancestor = scene->GetParentEntity(ancestor))
            std::erase(movedEntities, ancestor);

        if (history)
            history->BeginMove(scene, movedEntities);
        for (auto movedEntity : movedEntities) {
            movedEntity.SetParent(parent);
            sceneChanged = true;
        }
        if (history)
            history->EndMove("Move entities");

    }

    void SceneHierarchyPanel::RenderEntityRow(Ref<Scene::Scene>& scene, Scene::Entity entity, bool* selectionChanged) {

        renderedRows++;
        auto rowStart = ImGui::GetCursorScreenPos();
        auto rowStartY = rowStart.y;

        ImGuiTreeNodeFlags baseFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

        auto hierarchyComponent = entity.TryGetComponent<HierarchyComponent>();
        auto nameComponent = entity.TryGetComponent<NameComponent>();

        if (!hierarchyComponent) {
            baseFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        std::string nodeName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

        bool rootNode = hierarchyComponent && hierarchyComponent->root;

        auto nodeFlags = baseFlags;
        nodeFlags |= selectedEntities.contains(entity) ? ImGuiTreeNodeFlags_Selected : 0;

        bool nodeOpen = false;
        bool createEntity = false;
        bool deleteEntity = false;
        bool duplicateEntity = false;
        Scene::Entity droppedEntity;

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

        droppedEntity = AcceptDrop(scene, entity);

        if (ImGui::BeginPopupContextItem()) {
            auto getShortcut = [](const char* command) { return Singletons::commandPalette->GetShortcutText(command); };
            if (ImGui::MenuItem("Focus", getShortcut("Viewport: Focus selection").c_str()))
                focusRequested = true;

            if (!rootNode && nameComponent && ImGui::MenuItem("Rename", getShortcut("Hierarchy: Rename selected entity").c_str()))
                StartRename(entity);

            ImGui::Separator();

            if (ImGui::BeginMenu("Create child")) {
                createEntity = RenderCreateMenuItems(entity);
                ImGui::EndMenu();
            }

            if (!rootNode && ImGui::MenuItem("Duplicate entity", getShortcut("Hierarchy: Duplicate selection").c_str()))
                duplicateEntity = true;

            // We shouldn't allow the user to delete the root entity
            if (!rootNode && ImGui::MenuItem("Delete entity", getShortcut("Hierarchy: Delete selection").c_str()))
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

            // The open tree node pushed its id
            openRows.push_back({ entity, ImGui::GetCurrentWindow()->IDStack.back(), rowStart });
            TraverseChildren(scene, entity, selectionChanged);
            openRows.pop_back();

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

        if (droppedEntity.IsValid())
            MoveEntities(scene, entity, droppedEntity);

        // If the hierarchy is emtpy after movements or deletions, also remove the hierarchy
        hierarchyComponent = deleteEntity ? nullptr : entity.TryGetComponent<HierarchyComponent>();
        if (!rootNode && hierarchyComponent && hierarchyComponent->GetChildren().empty()) {
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

        if (Widgets::IconListItem(name.c_str(), Singletons::icons->Get(iconType), *selected)) {
            ClearSelection();
            *selected = true;
            *selectionChanged = true;
        }

    }

    uint32_t SceneHierarchyPanel::GetAllFilterBits() {

        return (1u << uint32_t(Scene::GetComponentTypes().size())) - 1u;

    }

    void SceneHierarchyPanel::RenderFilterPopup() {

        ImGui::SameLine();

        // Mark the button while a filter hides entities
        bool filterActive = (hierarchyFilter & GetAllFilterBits()) != GetAllFilterBits();
        if (Widgets::ToolbarIconButton("Filter button", Singletons::icons->Get(IconType::Filter),
            filterActive ? "Filter entities (active)" : "Filter entities"))
            ImGui::OpenPopup("Hierarchy filter settings");
        if (filterActive)
            Widgets::ActiveIndicator();

        if (ImGui::BeginPopup("Hierarchy filter settings")) {
            ImGui::SeparatorText("Show entities with");

            auto prevHierarchyFilter = hierarchyFilter;
            bool isSet = (hierarchyFilter & GetAllFilterBits()) == GetAllFilterBits();

            if (ImGui::Checkbox("All", &isSet)) {
                hierarchyFilter = isSet ? GetAllFilterBits() : 0;
            }
            ImGui::Separator();
            // Filters can be combined, so they are checkboxes
            const auto& componentTypes = Scene::GetComponentTypes();
            for (size_t i = 0; i < componentTypes.size(); i++) {
                auto bit = 1u << uint32_t(i);
                bool isBitSet = hierarchyFilter & bit;
                if (ImGui::Checkbox(componentTypes[i].name, &isBitSet))
                    hierarchyFilter = isBitSet ? hierarchyFilter | bit : hierarchyFilter & ~bit;
            }

            hierarchyFilterChanged = prevHierarchyFilter != hierarchyFilter;

            ImGui::EndPopup();
        }

    }

    void SceneHierarchyPanel::DeleteSelectedEntities(const Ref<Scene::Scene>& scene) {

        sceneChanged |= !selectedEntities.empty();

        // The root stays, e.g. when it is selected while pressing Delete
        std::vector<Scene::Entity> deletedEntities;
        for (auto ecsEntity : selectedEntities) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (entity.IsValid() && entity != scene->GetRootEntity())
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

    void SceneHierarchyPanel::DuplicateSelectedEntity(const Ref<Scene::Scene>& scene) {

        sceneChanged |= !selectedEntities.empty();

        auto oldSelectedEntites = selectedEntities;

        ClearSelection();

        if (history)
            history->BeginChange(scene, {});

        for (auto ecsEntity : oldSelectedEntites) {
            Scene::Entity entity(ecsEntity, &scene->entityManager);
            if (!entity.IsValid() || entity == scene->GetRootEntity())
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

        if ((data.filter & GetAllFilterBits()) != GetAllFilterBits()) {
            bool bitChecks = false;
            for (size_t i = 0; i < data.componentStorages.size() && !bitChecks; i++)
                bitChecks = (data.filter & (1u << uint32_t(i))) && data.componentStorages[i].Contains(entity);

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
