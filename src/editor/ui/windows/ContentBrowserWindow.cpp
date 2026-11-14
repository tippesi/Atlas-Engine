#include "ContentBrowserWindow.h"
#include "FileImporter.h"
#include "DataCreator.h"
#include "Notifications.h"
#include "ui/panels/PopupPanels.h"
#include "ui/Widgets.h"
#include "Theme.h"
#include "tools/CopyPasteHelper.h"
#include "tools/FileSystemHelper.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"
#include "loader/AssetLoader.h"

#include <filesystem>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#ifdef AE_OS_WINDOWS
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif

namespace Atlas::Editor::UI {

    ContentBrowserWindow::ContentBrowserWindow(bool show) : DrawerWindow("Content browser", show) {

        selectionStorage.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int idx) {
            (void)self;
            return uint32_t(idx);
        };

    }

    void ContentBrowserWindow::Update() {

        // Also while the browser is closed, the previews release what they don't need anymore
        previews.ReleaseUnused();

        // A search runs on the medium pool next to long jobs (e.g. navigation tiles), the window doesn't wait for it.
        // The next search starts once the last one has finished
        if (!show || isEditing || !searchAndFilterJob.HasFinished())
            return;

        auto contentDirectory = ContentDiscovery::GetDirectory(currentDirectory);
        if (!contentDirectory)
            return;

        // The job gets copies, the window changes the search while it runs
        auto searchQuery = assetSearch;
        std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
        auto filter = contentFilter;
        auto recursively = searchQuery.empty() ? filterRecursively && filter != ContentFilterBits::AllBit : searchRecursively;
        auto listDirectories = searchQuery.empty() && !filterRecursively && filter == ContentFilterBits::AllBit;

        JobSystem::Execute(searchAndFilterJob, [this, contentDirectory, searchQuery, filter, recursively, listDirectories](JobData&) {
            foundFiles.clear();
            SearchDirectory(contentDirectory, foundFiles, searchQuery, filter, recursively);
            std::sort(foundFiles.begin(), foundFiles.end(), [](const auto& file0, const auto& file1) {
                return file0.name < file1.name;
                });

            if (listDirectories)
                foundDirectories = contentDirectory->directories;
            else
                foundDirectories.clear();
            hasFoundEntries = true;
            });

    }

    void ContentBrowserWindow::Render() {

        // A pinned browser can be behind another tab when a file should be shown
        if (!contentToShowPath.empty())
            ImGui::SetNextWindowFocus();

        if (!Begin())
            return;

        const float minPanelWidth = 120.0f;
        auto panelWidth = std::clamp(folderPanelWidth, minPanelWidth,
            std::max(ImGui::GetWindowWidth() - 2.0f * minPanelWidth, minPanelWidth));

        ImGui::BeginChild("ResourceTypeSelection", ImVec2(panelWidth, 0.0f));

        // Same height as the toolbar on the right side
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Folders");

        ImGui::Separator();

        RenderDirectoryControl();

        ImGui::EndChild();

        ImGui::SameLine(0.0f, 0.0f);

        const float splitterWidth = 9.0f;
        auto splitterPos = ImGui::GetCursorScreenPos();
        auto splitterHeight = ImGui::GetContentRegionAvail().y;
        ImGui::InvisibleButton("Folder splitter", ImVec2(splitterWidth, std::max(splitterHeight, 1.0f)));
        if (ImGui::IsItemActive())
            folderPanelWidth = panelWidth + ImGui::GetIO().MouseDelta.x;

        auto splitterHighlighted = ImGui::IsItemActive() || ImGui::IsItemHovered();
        if (splitterHighlighted)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        auto splitterX = splitterPos.x + splitterWidth * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(splitterX, splitterPos.y), ImVec2(splitterX, splitterPos.y + splitterHeight),
            ImGui::GetColorU32(splitterHighlighted ? ImGuiCol_SeparatorActive : ImGuiCol_Separator), splitterHighlighted ? 2.0f : 1.0f);

        ImGui::SameLine(0.0f, 0.0f);

        ImGui::BeginChild("ResourceTypeOverview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        RenderDirectoryContentControl();

        ImGui::Separator();

        // Use a child as a dummy to create a drop target, since it doesn't work directly on a window
        // While Ctrl is held the mouse wheel zooms the icons instead of scrolling
        ImGui::BeginChild("ResourceTypeDropChild", ImVec2(0.0, 0.0), ImGuiChildFlags_None,
            ImGui::GetIO().KeyCtrl ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None);

        RenderDirectoryContent();

        ImGui::EndChild();

        if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            ImGui::SetWindowFocus();
        }

        if (ImGui::BeginDragDropTarget()) {
            auto dropPayload = ImGui::GetDragDropPayload();
            if (ImGui::AcceptDragDropPayload(typeid(Scene::Entity).name())) {
                Scene::Entity entity;
                std::memcpy(&entity, dropPayload->Data, dropPayload->DataSize);

                if (entity.IsValid()) {
                    auto scene = entity.GetScene();

                    auto& config = Singletons::config;
                    auto& scenes = config->openedScenes;

                    // The same scene, not one with the same name. Prefab windows have scenes of their own which aren't opened
                    auto iter = std::find_if(scenes.begin(), scenes.end(),
                        [&](const ResourceHandle<Scene::Scene>& item) { return item.IsLoaded() && item.Get().get() == scene; });
                    if (iter == scenes.end()) {
                        Notifications::Push({ .message = "Only entities of opened scenes can be saved as prefabs",
                            .type = NotificationType::Warning });
                    }
                    else {
                        auto sceneHandle = *iter;

                        auto nameComponent = entity.TryGetComponent<NameComponent>();
                        auto entityName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

                        auto dataDirectory = Loader::AssetLoader::GetDataDirectory();
                        auto assetPath = Common::Path::GetRelative(dataDirectory, currentDirectory) + "/";
                        if (assetPath.starts_with('/'))
                            assetPath.erase(assetPath.begin());

                        auto entityPath = assetPath + entityName + ".aeprefab";
                        Serializer::SerializePrefab(sceneHandle.Get(), entity, entityPath);
                    }
                }
            }

            ImGui::EndDragDropTarget();
        }

        hightlightPath.clear();

        // We can only set this up for the next frame
        if (!contentToShowPath.empty()) {
            // We expect the content to show to be relative to the asset directory
            currentDirectory =  Loader::AssetLoader::GetDataDirectory() + "/" 
                + Common::Path::GetDirectory(contentToShowPath);
            hightlightPath = contentToShowPath;
            selectionStorage.Clear();
        }
        contentToShowPath.clear();

        ImGui::EndChild();

        RenderDeleteConfirmation();

        // Generates the previews which were requested by the visible content
        previews.Update();

        End();

    }

    void ContentBrowserWindow::RenderDeleteConfirmation() {

        const char* popupName = "Delete content?";
        if (openDeleteConfirmation) {
            ImGui::OpenPopup(popupName);
            openDeleteConfirmation = false;
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (!ImGui::BeginPopupModal(popupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
            return;

        if (pendingDeletePaths.size() == 1)
            ImGui::Text("Delete %s?", std::filesystem::path(pendingDeletePaths.front()).filename().string().c_str());
        else
            ImGui::Text("Delete %d items?", int32_t(pendingDeletePaths.size()));

        // A few names, such that a wrong selection is noticed
        const size_t shownNames = 6;
        if (pendingDeletePaths.size() > 1) {
            for (size_t i = 0; i < std::min(shownNames, pendingDeletePaths.size()); i++)
                ImGui::TextColored(Theme::TextMuted(), "%s", std::filesystem::path(pendingDeletePaths[i]).filename().string().c_str());
            if (pendingDeletePaths.size() > shownNames)
                ImGui::TextColored(Theme::TextMuted(), "and %d more", int32_t(pendingDeletePaths.size() - shownNames));
        }

        ImGui::Spacing();
        ImGui::TextColored(Theme::Warning(), pendingDeleteHasDirectory ? "Folders are deleted with everything in them. This can't be undone" :
            "This can't be undone");
        ImGui::Spacing();

        auto buttonWidth = ImGui::CalcTextSize("Cancel").x * 2.5f;
        auto confirm = Widgets::DangerButton("Delete", ImVec2(buttonWidth, 0.0f));
        ImGui::SameLine();
        // Cancel is the safe choice for keyboard navigation
        auto cancel = ImGui::Button("Cancel", ImVec2(buttonWidth, 0.0f));
        ImGui::SetItemDefaultFocus();
        cancel |= ImGui::IsKeyPressed(ImGuiKey_Escape, false);

        if (confirm) {
            FileSystemHelper::Delete(pendingDeletePaths);
            selectionStorage.Clear();
        }
        if (confirm || cancel) {
            pendingDeletePaths.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();

    }

    void ContentBrowserWindow::RenderDirectoryControl() {

        auto rootDirectory = ContentDiscovery::GetContent();
        if (!rootDirectory)
            return;

        for (const auto& directory : rootDirectory->directories)
            RenderDirectoryEntry(directory);

    }

    void ContentBrowserWindow::RenderDirectoryContentControl() {

        auto dataDirectory = Loader::AssetLoader::GetDataDirectory();

        auto lineHeight = ImGui::GetTextLineHeight();

        auto normalizedDataDirectory = Common::Path::Normalize(dataDirectory);
        bool atRoot = Common::Path::Normalize(currentDirectory) == normalizedDataDirectory;

        ImGui::BeginDisabled(atRoot);
        if (Widgets::ToolbarIconButton("Back button", Singletons::icons->Get(IconType::ArrowLeft), "Go to parent folder")) {
            auto parentPath = std::filesystem::path(currentDirectory).parent_path();
            currentDirectory = Common::Path::Normalize(parentPath.string());
        }
        ImGui::EndDisabled();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        // Breadcrumbs: every part of the path can be clicked to jump there
        auto assetPath = Common::Path::GetRelative(dataDirectory, currentDirectory);
        assetPath = Common::Path::Normalize(assetPath);

        std::vector<std::string> segments;
        size_t segmentStart = 0;
        while (segmentStart <= assetPath.size()) {
            auto segmentEnd = assetPath.find('/', segmentStart);
            if (segmentEnd == std::string::npos)
                segmentEnd = assetPath.size();
            if (segmentEnd > segmentStart)
                segments.push_back(assetPath.substr(segmentStart, segmentEnd - segmentStart));
            segmentStart = segmentEnd + 1;
        }

        std::string crumbTarget;
        auto renderCrumb = [&](const std::string& label, const std::string& target, bool last, int32_t id) {
            ImGui::PushID(id);
            if (last) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label.c_str());
            }
            else if (ImGui::Button(label.c_str())) {
                crumbTarget = target;
            }
            ImGui::PopID();
            };

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, ImGui::GetStyle().ItemSpacing.y));

        ImGui::SameLine();
        renderCrumb("data", normalizedDataDirectory, segments.empty(), -1);

        auto accumulatedPath = normalizedDataDirectory;
        for (int32_t i = 0; i < int32_t(segments.size()); i++) {
            accumulatedPath += "/" + segments[i];

            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("/");
            ImGui::SameLine();
            renderCrumb(segments[i], accumulatedPath, i == int32_t(segments.size()) - 1, i);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (!crumbTarget.empty())
            currentDirectory = Common::Path::Normalize(crumbTarget);

        ImGui::SameLine(0.0f, 2.0f * padding);

        auto region = ImGui::GetContentRegionAvail();
        auto buttonSize = ImVec2(lineHeight, lineHeight);
        // A docked browser also has the button which turns it back into a drawer
        auto buttonCount = pinned ? 4.0f : 3.0f;

        // Wrap input text field into child, such that we can adjust the total size to not overlap anything
        ImGui::BeginChild("Search field", ImVec2(std::max(region.x - buttonCount * (buttonSize.x + 2.0f * padding), 100.0f), lineHeight + 2.0f * padding));
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##Search", "Search files...", &assetSearch);
        ImGui::EndChild();

        region = ImGui::GetContentRegionAvail();
        auto& viewIcon = Singletons::icons->Get(listView ? IconType::ViewGrid : IconType::ViewList);

        ImGui::SetCursorPos(ImVec2(region.x - buttonCount * (buttonSize.x + 2.0f * padding), 0.0f));
        if (Widgets::ToolbarIconButton("View button", viewIcon, listView ? "Show as tiles" : "Show as list"))
            listView = !listView;

        // Mark the button while a filter hides content
        bool filterActive = (contentFilter & ContentFilterBits::AllBit) != ContentFilterBits::AllBit;
        ImGui::SetCursorPos(ImVec2(region.x - (buttonCount - 1.0f) * (buttonSize.x + 2.0f * padding), 0.0f));
        if (Widgets::ToolbarIconButton("Filter button", Singletons::icons->Get(IconType::Filter),
            filterActive ? "Filter content (active)" : "Filter content"))
            ImGui::OpenPopup("Content browser filter settings");
        if (filterActive)
            Widgets::ActiveIndicator();

        if (ImGui::BeginPopup("Content browser filter settings")) {
            auto renderSingleBitOption = [&](const char* name, ContentFilterBits bit) {
                // Filters can be combined, so they are checkboxes
                bool isSet = contentFilter & bit;
                if (ImGui::Checkbox(name, &isSet)) {
                    contentFilter = isSet ? contentFilter | bit : contentFilter & ~bit;
                }
                };

            ImGui::SeparatorText("Show content of type");

            ContentFilter filtered = contentFilter & ContentFilterBits::AllBit;
            bool isSet = filtered == ContentFilterBits::AllBit;

            if (ImGui::Checkbox("All", &isSet)) {
                contentFilter = isSet ? ContentFilterBits::AllBit : 0;
            }
            ImGui::Separator();
            // The filter bits are in the order of the content types
            for (int32_t i = 0; i < static_cast<int32_t>(ContentType::None); i++)
                renderSingleBitOption(GetTypeName(static_cast<ContentType>(i)), static_cast<ContentFilterBits>(1 << i));

            ImGui::EndPopup();
        }

        region = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(region.x - (buttonCount - 2.0f) * (buttonSize.x + 2.0f * padding), 0.0f));
        if (Widgets::ToolbarIconButton("Settings button", Singletons::icons->Get(IconType::Settings), "Content browser settings"))
            ImGui::OpenPopup("Content browser settings");

        if (ImGui::BeginPopup("Content browser settings")) {

            ImGui::SeparatorText("Search");

            ImGui::Checkbox("Search recursively", &searchRecursively);
            ImGui::Checkbox("Filter recursively", &filterRecursively);

            ImGui::SeparatorText("Display");
            ImGui::Checkbox("Show previews", &showPreviews);
            ImGui::SetItemTooltip("Shows textures, meshes, materials, prefabs, fonts and audio instead of the icons of their type");
            ImGui::SliderFloat("Icon size", &iconSize, minIconSize, maxIconSize, "%.0f px");
            ImGui::TextDisabled("Ctrl + mouse wheel over the content also changes the size");

            ImGui::EndPopup();
        }

        if (pinned) {
            ImGui::SetCursorPos(ImVec2(region.x - (buttonSize.x + 2.0f * padding), 0.0f));
            RenderPinButton();
        }

    }

    void ContentBrowserWindow::RenderDirectoryContent() {

        float totalWidth = ImGui::GetContentRegionAvail().x;
        // Ctrl + mouse wheel zooms the icons
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::GetIO().KeyCtrl &&
            ImGui::GetIO().MouseWheel != 0.0f) {
            iconSize = std::clamp(iconSize + ImGui::GetIO().MouseWheel * 8.0f, minIconSize, maxIconSize);
        }

        auto columnCount = 1;
        float columnSize = totalWidth;
        gridStart = padding;

        // Tiles fill the width with small gaps, the icon size is their smallest size
        if (!listView) {
            auto available = totalWidth - 2.0f * tileGap;
            columnCount = std::max(1, int32_t((available + tileGap) / (iconSize + tileGap)));
            tileSize = std::floor((available - tileGap * float(columnCount - 1)) / float(columnCount));
            columnSize = tileSize + tileGap;
            gridStart = tileGap;
        }

        if (!std::filesystem::exists(currentDirectory)) {
            auto message = "Content directory " + Common::Path::Normalize(currentDirectory) + " has been moved or deleted.";
            Notifications::Push({ .message = message, .type = NotificationType::Warning });
            currentDirectory = Loader::AssetLoader::GetDataDirectory();
        }

        if (searchAndFilterJob.HasFinished() && hasFoundEntries) {
            directories.swap(foundDirectories);
            files.swap(foundFiles);
            hasFoundEntries = false;
        }

        auto dataDirectory = Loader::AssetLoader::GetDataDirectory();

        nextDirectory = std::string();

        auto entryCount = int32_t(directories.size()) + int32_t(files.size());
        ImGuiMultiSelectIO* multiSelectionIO = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_BoxSelect2d |
            ImGuiMultiSelectFlags_ClearOnClickVoid, selectionStorage.Size, entryCount);
        selectionStorage.ApplyRequests(multiSelectionIO);

        ImGui::SetCursorPosX(gridStart);

        int32_t entryIdx = 0;
        float columnHeight = 0.0f;
        for (const auto& directory : directories) {
            // Ignore 'invisible' directories
            if (directory->assetPath.at(0) == '.')
                continue;

            RenderContentEntry(directory->path, directory->assetPath, ContentType::None, 0, 0,
                entryIdx++, columnCount, columnSize, columnHeight);
        }

        for (const auto& file : files) {
            RenderContentEntry(file.path, file.assetPath, file.type, file.size, file.lastWriteTime,
                entryIdx++, columnCount, columnSize, columnHeight);
        }

        multiSelectionIO = ImGui::EndMultiSelect();
        selectionStorage.ApplyRequests(multiSelectionIO);

        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::BeginMenu("Create")) {
                auto path = Common::Path::GetAbsolute(currentDirectory);
                if (ImGui::MenuItem("Folder")) {
                    isEditing = true;
                    directories.push_back(CreateRef<ContentDirectory>({
                        .path = path,
                        .assetPath = path,
                        }));
                }
                if (ImGui::MenuItem("Material")) {
                    isEditing = true;
                    files.push_back(Content{
                        .path = path,
                        .assetPath = path,
                        .type = ContentType::Material,
                        });
                }
                if (ImGui::MenuItem("Material graph")) {
                    isEditing = true;
                    files.push_back(Content{
                        .path = path,
                        .assetPath = path,
                        .type = ContentType::MaterialGraph,
                        });
                }
                if (ImGui::MenuItem("Script")) {
                    isEditing = true;
                    files.push_back(Content{
                        .path = path,
                        .assetPath = path,
                        .type = ContentType::Script,
                        });
                }

                if (isEditing) {
                    editingChanged = true;
                    editingPath = path;
                    editingString.clear();
                    editingType = EditingType::Create;
                }
                ImGui::EndMenu();
            }

            if (CopyPasteHelper::AcceptPaste<ContentCopy>() && ImGui::MenuItem("Paste")) {
                ContentCopy copy;
                CopyPasteHelper::Paste(copy);
                FileSystemHelper::Copy(copy.paths, Common::Path::GetAbsolute(currentDirectory));
            }

            ImGui::EndPopup();
        }

        if (!nextDirectory.empty()) {
            currentDirectory = Common::Path::Normalize(nextDirectory);
        }

    }

    void ContentBrowserWindow::RenderDirectoryEntry(const Ref<ContentDirectory>& directory) {

        // Ignore 'invisible' directories
        if (directory->assetPath.at(0) == '.')
            return;

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

        if (currentDirectory == directory->path)
            nodeFlags |= ImGuiTreeNodeFlags_Selected;

        auto rowPosY = ImGui::GetCursorPosY();
        auto& style = ImGui::GetStyle();
        auto iconSize = ImGui::GetTextLineHeight();

        bool nodeOpen = ImGui::TreeNodeEx(directory->name.c_str(), nodeFlags, "");

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { 
            currentDirectory = Common::Path::Normalize(directory->path.string());
        }

        auto folderIcon = Singletons::icons->GetTintable(IconType::Folder);
        auto set = Singletons::imguiWrapper->GetTextureId(&folderIcon);

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::SetCursorPosY(rowPosY + style.FramePadding.y);
        ImGui::ImageWithBg(set, ImVec2(iconSize, iconSize), ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f),
            ImVec4(0.0f, 0.0f, 0.0f, 0.0f), Theme::ContentIcon(ContentType::None));

        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::SetCursorPosY(rowPosY);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(directory->name.c_str());

        if (nodeOpen) {
            for (const auto& childDirectory : directory->directories)
                RenderDirectoryEntry(childDirectory);

            ImGui::TreePop();
        }

    }

    void ContentBrowserWindow::RenderContentEntry(const std::filesystem::path& path, const std::string& assetPath, ContentType contentType,
        uintmax_t fileSize, int64_t lastWriteTime, int32_t entryIdx, int32_t columnCount, float columnSize, float& columnHeight) {

        bool isDirectory = contentType == ContentType::None;

        auto filename = Common::Path::GetFileName(assetPath);

        auto lineHeight = ImGui::GetTextLineHeight();
        ImVec2 buttonSize = listView ? ImVec2(columnSize - 2.0f * padding, lineHeight + 4.0f) : ImVec2(tileSize, GetTileHeight());
        auto cursorPos = ImGui::GetCursorPos();

        ImGui::BeginGroup();

        ImGui::PushID(entryIdx);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        Texture::Texture2D iconTexture;
        if (isDirectory)
            iconTexture = Singletons::icons->GetTintable(IconType::Folder);
        else
            iconTexture = GetIcon(contentType);
        auto set = Singletons::imguiWrapper->GetTextureId(&iconTexture);

        bool highlight = assetPath == hightlightPath;
        if (highlight) {
            ImGui::SetScrollY(ImGui::GetCursorPosY());
            selectionStorage.SetItemSelected((ImGuiID)entryIdx, true);
        }

        auto assetRelativePath = Common::Path::Normalize(assetPath);

        // Add selectable to allow for multi-select
        bool selected = selectionStorage.Contains((ImGuiID)entryIdx);
        ImGui::SetNextItemSelectionUserData(entryIdx);
        // Tiles draw their own background, hover and selection, the rectangle of the keyboard focus would be around
        // the larger area of the selectable
        if (!listView) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        }
        ImGui::Selectable("##Selectable", &selected, ImGuiSelectableFlags_None, buttonSize);
        if (!listView)
            ImGui::PopStyleColor(4);

        auto tileMin = ImGui::GetItemRectMin();
        auto tileHovered = ImGui::IsItemHovered();
        // Only after resting on an entry for a moment, moving over the content doesn't open tooltips
        auto showTooltip = ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary | ImGuiHoveredFlags_DelayNormal |
            ImGuiHoveredFlags_NoSharedDelay) && !ImGui::IsDragDropActive() && !isEditing;

        // Files are dragged right away instead of starting a box selection
        if (!isDirectory && ImGui::IsItemClicked())
            ImGui::GetCurrentContext()->BoxSelectState.IsStarting = false;

        if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                if (isDirectory)
                    nextDirectory = path.string();
                else if (contentType == ContentType::Scene)
                    FileImporter::ImportFile<Scene::Scene>(assetRelativePath);
                else if (contentType == ContentType::Mesh || contentType == ContentType::Animation ||
                    contentType == ContentType::Material || contentType == ContentType::Prefab ||
                    contentType == ContentType::MaterialGraph)
                    contentToEditPath = assetRelativePath;
            }
        }

        ImGui::SetItemAllowOverlap();

        if (!isDirectory && ImGui::BeginDragDropSource()) {
            // Size doesn't count the termination character, so add +1
            ImGui::SetDragDropPayload("ContentBrowserResource", assetRelativePath.data(), assetRelativePath.size() + 1);
            ImGui::TextUnformatted(filename.c_str());
            ImGui::TextDisabled("Drop on a matching resource slot");

            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginPopupContextItem()) {
            auto singleSelect = selectionStorage.Size == 1;
            // Do a direct import here without relying on the file importer
            if (contentType == ContentType::MeshSource && singleSelect && ImGui::MenuItem("Import as scene")) {
                PopupPanels::filename = assetRelativePath;
                PopupPanels::isImportScenePopupVisible = true;
            }

            auto hasAssetEditor = contentType == ContentType::Mesh || contentType == ContentType::Material ||
                contentType == ContentType::MaterialGraph || contentType == ContentType::Prefab;
            if (!isDirectory && hasAssetEditor && singleSelect && ImGui::MenuItem("Open"))
                contentToEditPath = assetRelativePath;
            if (!isDirectory && contentType == ContentType::Scene && singleSelect && ImGui::MenuItem("Open"))
                FileImporter::ImportFile<Scene::Scene>(assetRelativePath);

            // We shouldn't allow the user to delete the root entity
            if (singleSelect && ImGui::MenuItem("Open externally"))
                OpenExternally(std::filesystem::absolute(path).string(), isDirectory);

            if (singleSelect && ImGui::MenuItem("Rename")) {
                isEditing = true;
                auto dirEntryFilename = path.filename();
                editingString = dirEntryFilename.replace_extension("").string();
                editingChanged = true;
                editingPath = path;
                editingType = EditingType::Rename;
            }

            if (singleSelect && ImGui::MenuItem("Duplicate")) {
                FileSystemHelper::Duplicate(path.string());
            }

            if (ImGui::MenuItem("Copy")) {
                ContentCopy copy{ GetSelectedPaths() };
                CopyPasteHelper::Copy(copy);
            }

            // Deleting can't be undone, the window asks before anything is removed
            if (ImGui::MenuItem("Delete...")) {
                pendingDeletePaths = GetSelectedPaths();
                pendingDeleteHasDirectory = std::any_of(pendingDeletePaths.begin(), pendingDeletePaths.end(),
                    [](const std::string& path) { return std::filesystem::is_directory(path); });
                openDeleteConfirmation = true;
            }

            ImGui::EndPopup();
        }

        // Just draw the icon if it is actually visible
        bool isVisible = ImGui::IsItemVisible();
        if (isVisible) {
            ContentPreviews::Preview preview;
            auto hasPreview = !isDirectory && showPreviews &&
                previews.Get(assetRelativePath, contentType, fileSize, lastWriteTime, preview);

            if (listView) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(cursorPos.x);
                ImGui::SetCursorPosY(cursorPos.y + 3.0f);
                auto imageSize = ImVec2(lineHeight, lineHeight);
                if (hasPreview) {
                    RenderPreview(preview, contentType, imageSize, set);
                }
                else {
                    ImGui::ImageWithBg(set, imageSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                        ImVec4(0.0f, 0.0f, 0.0f, 0.0f), Theme::ContentIcon(contentType));
                }
            }
            else {
                auto editingThis = isEditing && editingPath == path;
                auto shortened = RenderTile(tileMin, editingThis ? std::string() : filename, contentType,
                    selected, tileHovered, set, hasPreview ? &preview : nullptr);
                if (isDirectory && shortened && showTooltip)
                    ImGui::SetTooltip("%s", filename.c_str());
            }

            if (!isDirectory && showTooltip)
                RenderContentTooltip(filename, contentType, fileSize, hasPreview ? &preview : nullptr, set);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        if (isEditing && editingPath == path) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

            if (listView)
                ImGui::SameLine();
            else
                ImGui::SetCursorScreenPos(ImVec2(tileMin.x + tileLabelPadding * 0.5f,
                    tileMin.y + tileSize));
            ImGui::SetNextItemWidth(listView ? columnSize * 0.5f : buttonSize.x - tileLabelPadding);
            if (editingChanged) {
                ImGui::SetKeyboardFocusHere();
            }
            if (ImGui::InputText("##Editing", &editingString, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                isEditing = false;
                ApplyEdit(contentType);
                ContentDiscovery::Execute();
            }
            // Only can apply a scroll after the element was specified above
            if (editingChanged && editingType == EditingType::Create) {
                ImGui::SetScrollY(ImGui::GetCursorPosY());
            }
            // Will not be immediately valid, so only check in next frame
            if ((!ImGui::IsItemFocused() || ImGui::IsItemDeactivated()) && !editingChanged) {
                isEditing = false;
                // Means we need to delete the last element (first the directory)
                if (editingType == EditingType::Create && contentType == ContentType::None) {
                    directories.pop_back();
                }
                else if (editingType == EditingType::Create && contentType != ContentType::None) {
                    files.pop_back();
                }
                ContentDiscovery::Execute();
            }
            editingChanged = false;

            ImGui::PopStyleColor();
            ImGui::PopStyleColor();

            // The field is inside the tile, the next row starts below the tile
            if (!listView) {
                ImGui::SetCursorScreenPos(ImVec2(tileMin.x, tileMin.y + buttonSize.y));
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
            }
        }
        else if (isVisible && listView) {
            RenderListColumns(filename, contentType, fileSize, buttonSize.x);
        }

        ImGui::PopID();

        ImGui::EndGroup();

        // Advance to next column, rows of tiles have the same gap as the columns
        columnHeight = std::max(listView ? ImGui::GetCursorPosY() - cursorPos.y : buttonSize.y + tileGap, columnHeight);

        ImGui::SetCursorPosX(((entryIdx + 1) % columnCount) * columnSize + gridStart);
        if ((entryIdx + 1) % columnCount == 0) {
            cursorPos.y = cursorPos.y + columnHeight;
            ImGui::SetCursorPosY(cursorPos.y);
            columnHeight = 0.0f;
        }
        else {
            ImGui::SetCursorPosY(cursorPos.y);
        }

    }

    void ContentBrowserWindow::RenderPreview(const ContentPreviews::Preview& preview, ContentType contentType, ImVec2 size,
        ImTextureID iconTexture) {

        auto min = ImGui::GetCursorScreenPos();
        ImGui::Dummy(size);

        DrawPreview(ImGui::GetWindowDrawList(), preview, contentType, min, ImVec2(min.x + size.x, min.y + size.y),
            iconTexture, true);

    }

    void ContentBrowserWindow::DrawPreview(ImDrawList* drawList, const ContentPreviews::Preview& preview, ContentType contentType,
        ImVec2 min, ImVec2 max, ImTextureID iconTexture, bool framed) {

        auto size = ImVec2(max.x - min.x, max.y - min.y);

        // The icon is replaced smoothly once the preview is generated
        if (preview.alpha < 1.0f) {
            auto inset = tileIconInset * std::min(size.x, size.y);
            drawList->AddImage(iconTexture, ImVec2(min.x + inset, min.y + inset), ImVec2(max.x - inset, max.y - inset),
                ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(Theme::ContentIcon(contentType, tileIconAlpha * (1.0f - preview.alpha))));
        }

        // Fonts and audio are only a shape, they get some room around them
        if (preview.tintable) {
            auto inset = 0.12f * std::min(size.x, size.y);
            min = ImVec2(min.x + inset, min.y + inset);
            max = ImVec2(max.x - inset, max.y - inset);
            size = ImVec2(max.x - min.x, max.y - min.y);
        }

        // Fitted into the area with the aspect ratio of the content
        auto scale = std::min(size.x / std::max(preview.size.x, 1.0f), size.y / std::max(preview.size.y, 1.0f));
        auto fitted = ImVec2(std::floor(preview.size.x * scale), std::floor(preview.size.y * scale));
        auto previewMin = ImVec2(std::floor(min.x + 0.5f * (size.x - fitted.x)), std::floor(min.y + 0.5f * (size.y - fitted.y)));
        auto previewMax = ImVec2(previewMin.x + fitted.x, previewMin.y + fitted.y);

        if (preview.tintable) {
            drawList->AddImage(preview.texture, previewMin, previewMax, preview.uvMin, preview.uvMax,
                ImGui::GetColorU32(Theme::ContentIcon(contentType, 0.85f * preview.alpha)));
        }
        else if (preview.transparent) {
            drawList->AddImage(preview.texture, previewMin, previewMax, preview.uvMin, preview.uvMax,
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, preview.alpha)));
        }
        else {
            auto rounding = framed ? std::clamp(std::min(fitted.x, fitted.y) * 0.06f, 1.0f, Theme::overlayRounding) : tileRounding - 2.0f;
            drawList->AddImageRounded(preview.texture, previewMin, previewMax, preview.uvMin, preview.uvMax,
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, preview.alpha)), rounding);
            // Dark images stand out from the background
            if (framed)
                drawList->AddRect(previewMin, previewMax, ImGui::GetColorU32(ImGuiCol_Border, 0.6f * preview.alpha), rounding);
        }

    }

    float ContentBrowserWindow::GetTileHeight() const {

        // Two lines for the name, the color of the type is shown by the line below the thumbnail
        return tileSize + 1.5f * tileLabelPadding + 2.0f * ImGui::GetTextLineHeight();

    }

    bool ContentBrowserWindow::RenderTile(ImVec2 min, const std::string& filename, ContentType contentType, bool selected,
        bool hovered, ImTextureID iconTexture, const ContentPreviews::Preview* preview) {

        auto drawList = ImGui::GetWindowDrawList();
        auto isDirectory = contentType == ContentType::None;
        auto lineHeight = ImGui::GetTextLineHeight();

        auto max = ImVec2(min.x + tileSize, min.y + GetTileHeight());
        auto thumbnailMax = ImVec2(min.x + tileSize, min.y + tileSize);
        auto labelWidth = tileSize - 2.0f * tileLabelPadding;

        if (isDirectory) {
            // Folders have no card, only a background while they are hovered or selected
            if (selected || hovered)
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(selected ? Theme::Accent(0.2f) : Theme::AssetCardHovered(0.7f)),
                    tileRounding);
            if (selected)
                drawList->AddRect(min, max, ImGui::GetColorU32(Theme::Accent(0.8f)), tileRounding, 0, 1.5f);

            auto inset = 0.1f * tileSize;
            drawList->AddImage(iconTexture, ImVec2(min.x + inset, min.y + inset), ImVec2(thumbnailMax.x - inset, thumbnailMax.y - inset),
                ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImGui::GetColorU32(Theme::ContentIcon(contentType)));

            return RenderTileName(drawList, filename, ImVec2(min.x + tileLabelPadding, thumbnailMax.y + tileLabelPadding * 0.5f),
                labelWidth, true);
        }

        // A soft card which lifts with a shadow when hovered, the thumbnail inside is darker such that previews
        // without background stand out
        if (hovered && !selected)
            drawList->AddRectFilled(ImVec2(min.x, min.y + 3.0f), ImVec2(max.x, max.y + 3.0f), ImGui::GetColorU32(Theme::Shadow(0.8f)), tileRounding);
        auto cardColor = selected ? Theme::Accent(0.16f) : (hovered ? Theme::AssetCardHovered() : Theme::AssetCard(0.6f));
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(cardColor), tileRounding);

        auto thumbnailMin = ImVec2(min.x + tileThumbnailInset, min.y + tileThumbnailInset);
        thumbnailMax = ImVec2(thumbnailMax.x - tileThumbnailInset, thumbnailMax.y - tileThumbnailInset);
        auto thumbnailSize = thumbnailMax.x - thumbnailMin.x;
        auto thumbnailRounding = tileRounding - 2.0f;
        drawList->AddRectFilled(thumbnailMin, thumbnailMax, ImGui::GetColorU32(Theme::AssetThumbnail()), thumbnailRounding);

        if (preview) {
            // Images fill the thumbnail, rendered content and shapes have their own margin
            DrawPreview(drawList, *preview, contentType, thumbnailMin, thumbnailMax, iconTexture, false);
        }
        else {
            auto inset = tileIconInset * thumbnailSize;
            drawList->AddImage(iconTexture, ImVec2(thumbnailMin.x + inset, thumbnailMin.y + inset),
                ImVec2(thumbnailMax.x - inset, thumbnailMax.y - inset), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                ImGui::GetColorU32(Theme::ContentIcon(contentType, tileIconAlpha)));
        }

        // A thin line in the color of the type along the bottom of the thumbnail
        drawList->AddRectFilled(ImVec2(thumbnailMin.x, thumbnailMax.y - tileTypeLineHeight), thumbnailMax,
            ImGui::GetColorU32(Theme::ContentTypeColor(contentType)), thumbnailRounding, ImDrawFlags_RoundCornersBottom);

        if (selected)
            drawList->AddRect(min, max, ImGui::GetColorU32(Theme::Accent()), tileRounding, 0, 1.5f);

        // The color says what the file is, the name doesn't need the extension
        auto extensionStart = filename.find_last_of('.');
        auto name = extensionStart != std::string::npos && extensionStart > 0 ? filename.substr(0, extensionStart) : filename;
        auto labelMin = ImVec2(min.x + tileLabelPadding, thumbnailMax.y + tileThumbnailInset + 0.5f * tileLabelPadding);
        auto shortened = RenderTileName(drawList, name, labelMin, labelWidth, false);

        return shortened;

    }

    bool ContentBrowserWindow::RenderTileName(ImDrawList* drawList, const std::string& name, ImVec2 position, float width, bool centered) {

        if (name.empty())
            return false;

        auto lineHeight = ImGui::GetTextLineHeight();

        const char* text = name.c_str();
        const char* textEnd = text + name.size();

        // As much as fits on the first line, the rest goes to the second one. Lines rather break after a separator
        // in the second half of the first line, e.g. M_Decal_ | 01. Names of several parts may break inside a part,
        // a single word ends with an ellipsis instead of being torn apart
        auto isSeparator = [](char character) { return character == '_' || character == '-' || character == ' ' || character == '.'; };

        auto firstLineEnd = text;
        while (firstLineEnd < textEnd) {
            unsigned int character;
            auto next = firstLineEnd + ImTextCharFromUtf8(&character, firstLineEnd, textEnd);
            if (ImGui::CalcTextSize(text, next).x > width)
                break;
            firstLineEnd = next;
        }
        if (firstLineEnd == text && text < textEnd) {
            unsigned int character;
            firstLineEnd += ImTextCharFromUtf8(&character, text, textEnd);
        }
        if (firstLineEnd < textEnd) {
            auto breakAt = static_cast<const char*>(nullptr);
            for (auto separator = firstLineEnd - 1; separator > text + (firstLineEnd - text) / 2; separator--) {
                if (isSeparator(*separator)) {
                    // A dot starts the second line, the others end the first one
                    breakAt = *separator == '.' ? separator : separator + 1;
                    break;
                }
            }
            if (!breakAt)
                breakAt = std::any_of(text, textEnd, isSeparator) ? firstLineEnd : textEnd;
            firstLineEnd = breakAt;
        }

        auto shortened = false;
        auto renderLine = [&](const char* lineStart, const char* lineEnd, float y) {
            auto textSize = ImGui::CalcTextSize(lineStart, lineEnd);
            auto lineWidth = std::min(textSize.x, width);
            auto x = centered ? position.x + 0.5f * (width - lineWidth) : position.x;
            auto lineMax = ImVec2(x + width, y + lineHeight);
            ImGui::RenderTextEllipsis(drawList, ImVec2(x, y), lineMax, std::min(lineMax.x, position.x + width),
                position.x + width, lineStart, lineEnd, &textSize);
            shortened |= textSize.x > width;
            };

        renderLine(text, firstLineEnd, position.y);
        if (firstLineEnd < textEnd)
            renderLine(firstLineEnd, textEnd, position.y + lineHeight);

        return shortened;

    }

    void ContentBrowserWindow::RenderContentTooltip(const std::string& filename, ContentType contentType, uintmax_t fileSize,
        const ContentPreviews::Preview* preview, ImTextureID iconTexture) {

        if (!ImGui::BeginTooltip())
            return;

        // A larger preview on the background of the thumbnails
        if (preview) {
            auto size = float(ContentPreviews::slotSize);
            auto min = ImGui::GetCursorScreenPos();
            auto max = ImVec2(min.x + size, min.y + size);
            ImGui::Dummy(ImVec2(size, size));

            auto drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(Theme::AssetThumbnail()), tileRounding);
            auto fullPreview = *preview;
            fullPreview.alpha = 1.0f;
            DrawPreview(drawList, fullPreview, contentType, min, max, iconTexture, false);
        }

        ImGui::TextUnformatted(filename.c_str());
        ImGui::TextColored(Theme::ContentTypeColor(contentType), "%s", GetTypeName(contentType));
        ImGui::SameLine();
        if (fileSize >= 1024 * 1024)
            ImGui::TextDisabled("%.1f MB", float(fileSize) / (1024.0f * 1024.0f));
        else
            ImGui::TextDisabled("%.1f KB", float(fileSize) / 1024.0f);

        ImGui::EndTooltip();

    }

    void ContentBrowserWindow::RenderListColumns(const std::string& filename, ContentType contentType, uintmax_t fileSize, float rowWidth) {

        auto startX = ImGui::GetCursorPosX();
        auto lineHeight = ImGui::GetTextLineHeight();

        // Everything is on the row of the icon, which already moved the cursor to the next line
        auto rowY = ImGui::GetCursorPosY() - lineHeight - ImGui::GetStyle().ItemSpacing.y - 1.0f;
        auto nameX = startX + lineHeight + ImGui::GetStyle().ItemInnerSpacing.x;
        auto typeX = startX + rowWidth * 0.6f;
        auto sizeX = startX + rowWidth * 0.85f;

        ImGui::SetCursorPos(ImVec2(nameX, rowY));
        // Files have a tooltip with their preview and name, shortened folder names get one of their own
        auto shortened = Widgets::TextEllipsis(filename.c_str(), filename.c_str() + filename.size(), typeX - nameX - padding);
        if (shortened && contentType == ContentType::None && ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary |
            ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
            ImGui::SetTooltip("%s", filename.c_str());

        ImGui::SetCursorPos(ImVec2(typeX, rowY));
        ImGui::TextDisabled("%s", contentType == ContentType::None ? "Folder" : GetTypeName(contentType));

        if (contentType != ContentType::None) {
            ImGui::SetCursorPos(ImVec2(sizeX, rowY));
            if (fileSize >= 1024 * 1024)
                ImGui::TextDisabled("%.1f MB", float(fileSize) / (1024.0f * 1024.0f));
            else
                ImGui::TextDisabled("%.1f KB", float(fileSize) / 1024.0f);
        }

    }

    const char* ContentBrowserWindow::GetTypeName(ContentType contentType) const {

        switch (contentType) {
        case ContentType::Audio: return "Audio";
        case ContentType::Mesh: return "Mesh";
        case ContentType::MeshSource: return "Mesh source";
        case ContentType::Material: return "Material";
        case ContentType::Terrain: return "Terrain";
        case ContentType::Scene: return "Scene";
        case ContentType::Script: return "Script";
        case ContentType::Font: return "Font";
        case ContentType::Prefab: return "Prefab";
        case ContentType::Texture: return "Texture";
        case ContentType::EnvironmentTexture: return "Environment texture";
        case ContentType::Animation: return "Animation";
        case ContentType::MaterialGraph: return "Material graph";
        default: return "File";
        }

    }

    bool ContentBrowserWindow::IsValidFileType(const std::string& filename) {

        std::string fileType = Common::Path::GetFileType(filename);
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

        return Content::contentTypeMapping.contains(fileType);

    }

    Texture::Texture2D& ContentBrowserWindow::GetIcon(const ContentType contentType) {

        auto& icons = Singletons::icons;

        switch (contentType) {
        case ContentType::Audio: return icons->GetTintable(IconType::Audio);
        case ContentType::Mesh: return icons->GetTintable(IconType::Mesh);
        case ContentType::MeshSource: return icons->GetTintable(IconType::MeshSource);
        case ContentType::Material: return icons->GetTintable(IconType::Material);
        case ContentType::MaterialGraph: return icons->GetTintable(IconType::MaterialGraph);
        case ContentType::Scene: return icons->GetTintable(IconType::Scene);
        case ContentType::Font: return icons->GetTintable(IconType::Font);
        case ContentType::Prefab: return icons->GetTintable(IconType::Prefab);
        case ContentType::Texture: return icons->GetTintable(IconType::Image);
        case ContentType::EnvironmentTexture: return icons->GetTintable(IconType::EnvironmentImage);
        case ContentType::Terrain: return icons->GetTintable(IconType::Terrain);
        default: return icons->GetTintable(IconType::Document);
        }

    }

    void ContentBrowserWindow::SearchDirectory(const Ref<ContentDirectory>& directory, std::vector<Content>& contentFiles,
        const std::string& searchQuery, ContentFilter filter, bool recursively) {

        for (const auto& file : directory->files) {
            // Enum class and ContentFilterBits are in same order, so we can just bitshift the content type.
            if (!((1 << static_cast<uint32_t>(file.type)) & filter)) {
                continue;
            }

            if (!searchQuery.empty() && file.name.find(searchQuery) == std::string::npos) {
                continue;
            }

            contentFiles.push_back(file);
        }

        if (recursively) {
            for (const auto& childDirectory : directory->directories) {
                SearchDirectory(childDirectory, contentFiles, searchQuery, filter, true);
            }
        }

    }

    void ContentBrowserWindow::OpenExternally(const std::string& path, bool isDirectory) {

#ifdef AE_OS_WINDOWS
        ShellExecute(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
#endif
#if defined(AE_OS_LINUX) || defined(AE_OS_MACOS)
        auto command = "open " + path;
        system(command.c_str());
#endif

    }

    std::vector<std::string> ContentBrowserWindow::GetSelectedPaths() {

        std::vector<std::string> paths;

        int32_t entryIdx = 0;
        for (const auto& directory : directories) {
            // Ignore 'invisible' directories
            if (directory->assetPath.at(0) == '.')
                continue;

            if (selectionStorage.Contains(entryIdx++))
                paths.push_back(directory->path.string());
        }

        for (const auto& file : files) {
            if (selectionStorage.Contains(entryIdx++))
                paths.push_back(file.path.string());
        }

        return paths;

    }

    void ContentBrowserWindow::ApplyEdit(ContentType type) {

        if (editingType == EditingType::Rename) {
            auto newPath = editingPath;
            newPath = newPath.replace_filename(editingString);
            if (editingPath.has_extension())
                newPath = newPath.replace_extension(editingPath.extension());
            std::filesystem::rename(editingPath, newPath);
            return;
        }

        if (type == ContentType::None) {
            auto dirPath = editingPath;
            dirPath.append(editingString);
            std::filesystem::create_directory(dirPath);
            return;
        }

        auto path = editingPath;
        path.append(editingString);

        // All the file types with a loader
        if (type == ContentType::Material) {
            path.replace_extension("aematerial");            
            Loader::MaterialLoader::SaveMaterial(CreateRef<Material>(), path.string());
            return;
        }

        if (type == ContentType::MaterialGraph) {
            path.replace_extension("aematgraph");
            Loader::MaterialGraphLoader::SaveGraph(MaterialGraph::Graph::CreateDefault(), path.string());
            return;
        }

        // All the files with empty contents
        if (type == ContentType::Script) {
            path.replace_extension("lua");
        }
        auto assetRelative = Common::Path::GetAbsolute(path.string());
        auto fileStream = Loader::AssetLoader::WriteFile(assetRelative, std::ios::out | std::ios::binary);
        if (!fileStream.is_open()) {
            Notifications::Push({"Couldn't write file " + 
                Loader::AssetLoader::GetRelativePath(assetRelative), NotificationType::Error});
            return;
        }
        fileStream.close();

    }

}
