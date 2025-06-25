#include "ContentBrowserWindow.h"
#include "FileImporter.h"
#include "DataCreator.h"
#include "Notifications.h"
#include "ui/panels/PopupPanels.h"
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

    ContentBrowserWindow::ContentBrowserWindow(bool show) : Window("Content browser", show) {

        selectionStorage.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int idx) { return uint32_t(idx); };

    }

    void ContentBrowserWindow::Update() {

        if (!show)
            return;

        JobSystem::Execute(searchAndFilterJob, [&](JobData&) {
            UpdateFilteredAndSortedDirEntries();
            });

    }

    void ContentBrowserWindow::Render() {

        if (!Begin())
            return;

        ImGuiID dsID = ImGui::GetID(dockSpaceNameID.c_str());
        auto viewport = ImGui::GetWindowViewport();

        if (!ImGui::DockBuilderGetNode(dsID) || resetDockingLayout) {
            ImGui::DockBuilderRemoveNode(dsID);
            ImGui::DockBuilderAddNode(dsID, ImGuiDockNodeFlags_DockSpace);

            ImGui::DockBuilderSetNodeSize(dsID, viewport->Size);

            uint32_t dockIdLeft, dockIdRight;
            ImGui::DockBuilderSplitNode(dsID, ImGuiDir_Left, 0.05f, &dockIdLeft, &dockIdRight);

            ImGuiDockNode* leftNode = ImGui::DockBuilderGetNode(dockIdLeft);
            ImGuiDockNode* rightNode = ImGui::DockBuilderGetNode(dockIdRight);
            leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoDockingOverMe;

            // we now dock our windows into the docking node we made above
            ImGui::DockBuilderDockWindow("ResourceTypeSelection", dockIdLeft);
            ImGui::DockBuilderDockWindow("ResourceTypeOverview", dockIdRight);
            ImGui::DockBuilderFinish(dsID);

            resetDockingLayout = false;
        }

        ImGui::DockSpace(dsID, ImVec2(0.0f, 0.0f), 0);

        End();

        ImGui::Begin("ResourceTypeSelection", nullptr);

        ImGui::SetWindowFontScale(1.5f);

        ImGui::Text("Folders");

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Separator();

        RenderDirectoryControl();

        ImGui::End();

        ImGui::Begin("ResourceTypeOverview", nullptr);

        // Use a child as a dummy to create a drop target, since it doesn't work directly on a window
        ImGui::BeginChild("ResourceTypeDropChild", ImVec2(0.0, 0.0));

        RenderDirectoryContentControl();

        ImGui::Separator();

        RenderDirectoryContent();

        ImGui::EndChild();

        if (ImGui::IsDragDropActive() && ImGui::IsWindowHovered()) {
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

                    // There must be a scene, otherwise we want to get a fatal error
                    auto iter = std::find_if(scenes.begin(), scenes.end(),
                        [&](const ResourceHandle<Scene::Scene>& item) -> bool {
                            if (!item.IsLoaded())
                                return false;
                            return item->name == scene->name;
                        });
                    auto sceneHandle = *iter;

                    auto nameComponent = entity.TryGetComponent<NameComponent>();
                    auto entityName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

                    auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();
                    auto assetPath = Common::Path::GetRelative(assetDirectory, currentDirectory) + "/";
                    if (assetPath.starts_with('/'))
                        assetPath.erase(assetPath.begin());

                    auto entityPath = assetPath + entityName + ".aeprefab";
                    Serializer::SerializePrefab(sceneHandle.Get(), entity, entityPath);
                }
            }

            ImGui::EndDragDropTarget();
        }

        hightlightPath.clear();

        // We can only set this up for the next frame
        if (!contentToShowPath.empty()) {
            // We expect the content to show to be relative to the asset directory
            currentDirectory =  Loader::AssetLoader::GetAssetDirectory() + "/" 
                + Common::Path::GetDirectory(contentToShowPath);
            hightlightPath = contentToShowPath;
            selectionStorage.Clear();
        }
        contentToShowPath.clear();

        ImGui::End();

    }

    void ContentBrowserWindow::RenderDirectoryControl() {

        auto rootDirectory = ContentDiscovery::GetContent();
        if (!rootDirectory)
            return;

        for (const auto& directory : rootDirectory->directories)
            RenderDirectoryEntry(directory);

    }

    void ContentBrowserWindow::RenderDirectoryContentControl() {

        auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();

        ImGui::SetWindowFontScale(1.5f);

        auto lineHeight = ImGui::GetTextLineHeight();
        auto set = Singletons::imguiWrapper->GetTextureId(&Singletons::icons->Get(IconType::ArrowLeft));

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        if (ImGui::ImageButton("Back button", set, ImVec2(lineHeight, lineHeight), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f))) {
            if (currentDirectory != assetDirectory) {
                auto parentPath = std::filesystem::path(currentDirectory).parent_path();
                currentDirectory = Common::Path::Normalize(parentPath.string());
            }
        }

        ImGui::PopStyleColor();

        ImGui::SameLine();

        // Being in a child window offsets everything, need to align the // slashes
        ImGui::SetCursorPosY(-2.0f);

        auto assetPath = Common::Path::GetRelative(assetDirectory, currentDirectory);
        assetPath = Common::Path::Normalize(assetPath);
        if (assetPath.starts_with('/'))
            assetPath.erase(assetPath.begin());

        assetPath = "//:" + assetPath;
        ImGui::Text("%s", assetPath.c_str());

        ImGui::SameLine();

        ImGui::SetWindowFontScale(1.0f);

        auto region = ImGui::GetContentRegionAvail();
        lineHeight = ImGui::GetTextLineHeight();
        auto buttonSize = ImVec2(lineHeight, lineHeight);

        // Wrap input text field into child, such that we can adjust the total size to not overlap anything
        ImGui::BeginChild("Search field", ImVec2(region.x - 2.0f * (buttonSize.x + 2.0f * padding), lineHeight + 2.0f * padding));
        ImGui::InputTextWithHint("Search", "Type to search for files", &assetSearch);
        ImGui::EndChild();

        region = ImGui::GetContentRegionAvail();
        auto& filterIcon = Singletons::icons->Get(IconType::Filter);
        set = Singletons::imguiWrapper->GetTextureId(&filterIcon);

        auto uvMin = ImVec2(0.1f, 0.1f);
        auto uvMax = ImVec2(0.9f, 0.9f);

        ImGui::SetCursorPos(ImVec2(region.x - 2.0f * (buttonSize.x + 2.0f * padding), 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::ImageButton("Filter button", set, buttonSize, uvMin, uvMax)) {
            ImGui::OpenPopup("Content browser filter settings");
        }
        ImGui::PopStyleColor();

        if (ImGui::BeginPopup("Content browser filter settings")) {
            auto renderSingleBitOption = [&](const char* name, ContentFilterBits bit) {
                bool isSet = contentFilter & bit;
                if (ImGui::RadioButton(name, isSet)) {
                    contentFilter = isSet ? contentFilter & ~bit : contentFilter | bit;
                }
                };

            ImGui::Text("Filters");
            ImGui::Separator();

            ContentFilter filtered = contentFilter & ContentFilterBits::AllBit;
            bool isSet = filtered == ContentFilterBits::AllBit;

            if (ImGui::RadioButton("All", isSet)) {
                contentFilter = isSet ? 0 : ContentFilterBits::AllBit;
            }
            renderSingleBitOption("Audio", ContentFilterBits::AudioBit);
            renderSingleBitOption("Mesh", ContentFilterBits::MeshBit);
            renderSingleBitOption("Mesh source", ContentFilterBits::MeshSourceBit);
            renderSingleBitOption("Material", ContentFilterBits::MaterialBit);
            renderSingleBitOption("Terrain", ContentFilterBits::TerrainBit);
            renderSingleBitOption("Scene", ContentFilterBits::SceneBit);
            renderSingleBitOption("Script", ContentFilterBits::ScriptBit);
            renderSingleBitOption("Font", ContentFilterBits::FontBit);
            renderSingleBitOption("Prefab", ContentFilterBits::PrefabBit);
            renderSingleBitOption("Texture", ContentFilterBits::TextureBit);
            renderSingleBitOption("Environment texture", ContentFilterBits::EnvTextureBit);

            ImGui::EndPopup();
        }

        region = ImGui::GetContentRegionAvail();
        auto& settingsIcon = Singletons::icons->Get(IconType::Settings);
        set = Singletons::imguiWrapper->GetTextureId(&settingsIcon);

        ImGui::SetCursorPos(ImVec2(region.x - (buttonSize.x + 2.0f * padding), 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::ImageButton("Settings button", set, buttonSize, uvMin, uvMax)) {
            ImGui::OpenPopup("Content browser settings");
        }
        ImGui::PopStyleColor();

        if (ImGui::BeginPopup("Content browser settings")) {

            ImGui::Text("Content settings");
            ImGui::Separator();

            ImGui::Checkbox("Search recursively", &searchRecursively);
            ImGui::Checkbox("Filter recursively", &filterRecursively);

            ImGui::EndPopup();
        }

    }

    void ContentBrowserWindow::RenderDirectoryContent() {

        float totalWidth = ImGui::GetContentRegionAvail().x;
        auto columnCount = int32_t(totalWidth / itemSize);
        columnCount = columnCount <= 0 ? 1 : columnCount;

        float columnSize = totalWidth / float(columnCount);

        if (!std::filesystem::exists(currentDirectory)) {
            auto message = "Content directory " + Common::Path::Normalize(currentDirectory) + " has been moved or deleted.";
            Notifications::Push({ .message = message, .color = vec3(1.0f, 1.0f, 0.0f) });
            currentDirectory = Loader::AssetLoader::GetAssetDirectory();
        }

        JobSystem::Wait(searchAndFilterJob);

        auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();

        nextDirectory = std::string();

        auto entryCount = int32_t(directories.size()) + int32_t(files.size());
        ImGuiMultiSelectIO* multiSelectionIO = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_BoxSelect2d |
            ImGuiMultiSelectFlags_ClearOnClickVoid, selectionStorage.Size, entryCount);
        selectionStorage.ApplyRequests(multiSelectionIO);

        ImGui::SetCursorPosX(padding);

        int32_t entryIdx = 0;
        float columnHeight = 0.0f;
        for (const auto& directory : directories) {
            // Ignore 'invisible' directories
            if (directory->assetPath.at(0) == '.')
                continue;

            RenderContentEntry(directory->path, directory->assetPath, ContentType::None,
                entryIdx++, columnCount, columnSize, columnHeight);
        }

        for (const auto& file : files) {
            RenderContentEntry(file.path, file.assetPath, file.type,
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

        // Get button size before removing spacing
        float buttonSize = ImGui::GetTextLineHeightWithSpacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-2.0f, 0.0f));

        bool nodeOpen = ImGui::TreeNodeEx(directory->name.c_str(), nodeFlags, "");

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { 
            currentDirectory = Common::Path::Normalize(directory->path.string());
        }

        ImGui::SameLine();

        auto folderIcon = Singletons::icons->Get(IconType::Folder);
        auto set = Singletons::imguiWrapper->GetTextureId(&folderIcon);

        ImGui::Image(set, ImVec2(buttonSize, buttonSize), ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f));

        ImGui::PopStyleVar();

        ImGui::SameLine();

        ImGui::Text("%s", directory->name.c_str());

        if (nodeOpen) {
            for (const auto& childDirectory : directory->directories)
                RenderDirectoryEntry(childDirectory);

            ImGui::TreePop();
        }

    }

    void ContentBrowserWindow::RenderContentEntry(const std::filesystem::path& path, const std::string& assetPath,
        ContentType contentType, int32_t entryIdx, int32_t columnCount, float columnSize, float& columnHeight) {

        bool isDirectory = contentType == ContentType::None;

        auto filename = Common::Path::GetFileName(assetPath);

        ImVec2 buttonSize = ImVec2(iconSize, iconSize);
        auto cursorPos = ImGui::GetCursorPos();

        ImGui::BeginGroup();

        ImGui::PushID(entryIdx);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        Texture::Texture2D iconTexture;
        if (isDirectory)
            iconTexture = Singletons::icons->Get(IconType::Folder);
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
        ImGui::Selectable("##Selectable", &selected, ImGuiSelectableFlags_None, buttonSize);

        if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseDoubleClicked(0)) {
                if (isDirectory)
                    nextDirectory = path.string();
                else if (contentType == ContentType::Scene)
                    FileImporter::ImportFile<Scene::Scene>(assetRelativePath);
            }
        }

        ImGui::SetItemAllowOverlap();

        if (!isDirectory && ImGui::BeginDragDropSource()) {
            // Size doesn't count the termination character, so add +1
            ImGui::SetDragDropPayload("ContentBrowserResource", assetRelativePath.data(), assetRelativePath.size() + 1);
            ImGui::Text("Drag to entity component");

            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginPopupContextItem()) {
            auto singleSelect = selectionStorage.Size == 1;
            // Do a direct import here without relying on the file importer
            if (contentType == ContentType::MeshSource && singleSelect && ImGui::MenuItem("Import as scene")) {
                PopupPanels::filename = assetRelativePath;
                PopupPanels::isImportScenePopupVisible = true;
            }

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

            if (ImGui::MenuItem("Delete")) {
                auto paths = GetSelectedPaths();
                FileSystemHelper::Delete(paths);
            }

            ImGui::EndPopup();
        }

        // Just draw the icon if it is actually visible
        if (ImGui::IsItemVisible()) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(cursorPos.x);
            ImGui::Image(set, buttonSize);
        }

        ImGui::PopStyleColor();

        if (isEditing && editingPath == path) {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

            ImGui::SetNextItemWidth(buttonSize.x);
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
        }
        else {
            auto offset = 0.0f;
            auto textSize = ImGui::CalcTextSize(filename.c_str());
            if (textSize.x < iconSize)
                offset = (iconSize - textSize.x) / 2.0f;
            auto cursorX = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(cursorX + offset);
            ImGui::PushTextWrapPos(cursorX + buttonSize.x);
            ImGui::Text("%s", filename.c_str());
            ImGui::PopTextWrapPos();
        }

        ImGui::PopID();

        ImGui::EndGroup();

        // Advance to next column
        columnHeight = std::max(ImGui::GetCursorPosY() - cursorPos.y, columnHeight);

        ImGui::SetCursorPosX(((entryIdx + 1) % columnCount) * columnSize + padding);
        if ((entryIdx + 1) % columnCount == 0) {
            cursorPos.y = cursorPos.y + columnHeight;
            ImGui::SetCursorPosY(cursorPos.y);
            columnHeight = 0.0f;
        }
        else {
            ImGui::SetCursorPosY(cursorPos.y);
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
        case ContentType::Audio: return icons->Get(IconType::Audio);
        case ContentType::Mesh: return icons->Get(IconType::Mesh);
        case ContentType::MeshSource: return icons->Get(IconType::MeshSource);
        case ContentType::Material: return icons->Get(IconType::Material);
        case ContentType::Scene: return icons->Get(IconType::Scene);
        case ContentType::Font: return icons->Get(IconType::Font);
        case ContentType::Prefab: return icons->Get(IconType::Prefab);
        case ContentType::Texture: return icons->Get(IconType::Image);
        case ContentType::EnvironmentTexture: return icons->Get(IconType::EnvironmentImage);
        case ContentType::Terrain: return icons->Get(IconType::Terrain);
        default: return icons->Get(IconType::Document);
        }

    }

    void ContentBrowserWindow::UpdateFilteredAndSortedDirEntries() {

        // Don't want to update anything now
        if (isEditing)
            return;

        auto contentDirectory = ContentDiscovery::GetDirectory(currentDirectory);
        if (!contentDirectory)
            return;

        std::string searchQuery = assetSearch;
        if (!searchQuery.empty()) {
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
        }

        std::vector<Content> discoverdFiles;
        bool recursively = searchQuery.empty() ? filterRecursively && contentFilter != ContentFilterBits::AllBit : searchRecursively;
        SearchDirectory(contentDirectory, discoverdFiles, searchQuery, recursively);

        std::sort(discoverdFiles.begin(), discoverdFiles.end(), [](const auto& file0, const auto& file1) {
            return file0.name < file1.name;
            });

        if (assetSearch.empty() && !filterRecursively && contentFilter == ContentFilterBits::AllBit) {
            directories = contentDirectory->directories;
            files = discoverdFiles;
        }
        else {
            files = discoverdFiles;
            directories.clear();
        }

    }

    void ContentBrowserWindow::SearchDirectory(const Ref<ContentDirectory>& directory, std::vector<Content>& contentFiles,
        const std::string& searchQuery, bool recursively) {

        for (const auto& file : directory->files) {
            // Enum class and ContentFilterBits are in same order, so we can just bitshift the content type
            if (!((1 << static_cast<uint32_t>(file.type)) & contentFilter)) {
                continue;
            }

            if (!searchQuery.empty() && file.name.find(searchQuery) == std::string::npos) {
                continue;
            }

            contentFiles.push_back(file);
        }

        if (recursively) {
            for (const auto& childDirectory : directory->directories) {
                SearchDirectory(childDirectory, contentFiles, searchQuery, true);
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

        // All the files with empty contents
        if (type == ContentType::Script) {
            path.replace_extension("lua");
        }
        auto assetRelative = Common::Path::GetAbsolute(path.string());
        auto fileStream = Loader::AssetLoader::WriteFile(assetRelative, std::ios::out | std::ios::binary);
        if (!fileStream.is_open()) {
            Notifications::Push({"Couldn't write file " + 
                Loader::AssetLoader::GetRelativePath(assetRelative), vec3(1.0f, 0.0f, 0.0f)});
            return;
        }
        fileStream.close();

    }

}