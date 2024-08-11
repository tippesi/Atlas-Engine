#include "ContentBrowserWindow.h"
#include "FileImporter.h"
#include "DataCreator.h"
#include "Notifications.h"
#include "ui/panels/PopupPanels.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"
#include "loader/AssetLoader.h"

#include <filesystem>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#ifdef AE_OS_WINDOWS
#include <Windows.h>
#include <shellapi.h>
#endif

namespace Atlas::Editor::UI {

    ContentBrowserWindow::ContentBrowserWindow(bool show) : Window("Content browser", show) {



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

        ImGui::Text("Filters");

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Separator();

        const char* items[] = { "Audio", "Mesh", "Mesh source", "Material", "Terrain", "Scene",
            "Script", "Font", "Prefab", "Texture", "Environment texture" };
        for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
            bool isSelected = selectedFilter == i;
            ImGui::Selectable(items[i], &isSelected, ImGuiSelectableFlags_SpanAvailWidth);
            if (isSelected) {
                selectedFilter = i;
            }
            else if (selectedFilter == i) {
                selectedFilter = -1;
            }
        }

        ImGui::End();

        ImGui::Begin("ResourceTypeOverview", nullptr);

        // Use a child as a dummy to create a drop target, since it doesn't work directly on a window
        ImGui::BeginChild("ResourceTypeDropChild");

        RenderDirectoryControl();

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

        ImGui::End();

    }

    void ContentBrowserWindow::RenderDirectoryControl() {

        auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();

        ImGui::SetWindowFontScale(1.5f);

        auto lineHeight = ImGui::GetTextLineHeight();
        auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(Singletons::icons->Get(IconType::ArrowLeft));

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        if (ImGui::ImageButton(set, ImVec2(lineHeight, lineHeight), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), 0)) {
            if (currentDirectory != assetDirectory) {
                auto parentPath = std::filesystem::path(currentDirectory).parent_path();
                currentDirectory = parentPath.string();
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

        ImGui::InputTextWithHint("Search", "Type to search for files", &assetSearch);

        auto region = ImGui::GetContentRegionAvail();
        auto& moreIcon = Singletons::icons->Get(IconType::Settings);
        set = Singletons::imguiWrapper->GetTextureDescriptorSet(moreIcon);

        lineHeight = ImGui::GetTextLineHeight();
        auto buttonSize = ImVec2(lineHeight, lineHeight);

        auto uvMin = ImVec2(0.1f, 0.1f);
        auto uvMax = ImVec2(0.9f, 0.9f);

        ImGui::SetCursorPos(ImVec2(region.x - (buttonSize.x + 2.0f * padding), 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::ImageButton(set, buttonSize, uvMin, uvMax)) {
            ImGui::OpenPopup("Content browser settings");
        }
        ImGui::PopStyleColor();

        if (ImGui::BeginPopup("Content browser settings")) {
            auto& settings = Singletons::config->contentBrowserSettings;

            ImGui::Checkbox("Search recursively", &settings.searchRecursively);
            ImGui::Checkbox("Filter recursively", &settings.filterRecursively);

            ImGui::EndPopup();
        }

    }

    void ContentBrowserWindow::RenderDirectoryContent() {

        float totalWidth = ImGui::GetContentRegionAvail().x;
        auto columnItemCount = int32_t(totalWidth / itemSize);
        columnItemCount = columnItemCount <= 0 ? 1 : columnItemCount;

        ImGui::Columns(columnItemCount, nullptr, false);

        if (!std::filesystem::exists(currentDirectory)) {
            auto message = "Content directory " + Common::Path::Normalize(currentDirectory) + " has been moved or deleted.";
            Notifications::Push({ .message = message, .color = vec3(1.0f, 1.0f, 0.0f) });
            currentDirectory = Loader::AssetLoader::GetAssetDirectory();
        }

        UpdateFilteredAndSortedDirEntries();

        auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();

        nextDirectory = std::string();

        for (const auto& directory : directories) {
            RenderContentEntry(directory->path, directory->assetPath, ContentType::None);
        }

        for (const auto& file : files) {
            RenderContentEntry(file.path, file.assetPath, file.type);
        }

        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight)) {
            if (ImGui::BeginMenu("Create")) {
                if (ImGui::MenuItem("Folder")) {

                }
                if (ImGui::MenuItem("Script")) {

                }
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        if (TextInputPopup("Rename item", renamePopupVisible, renameString)) {
            auto newPath = renamePath;
            newPath = newPath.replace_filename(renameString);
            if (renamePath.has_extension())
                newPath = newPath.replace_extension(renamePath.extension());
            std::filesystem::rename(renamePath, newPath);
        }

        if (!nextDirectory.empty()) {
            currentDirectory = nextDirectory;
        }

    }

    void ContentBrowserWindow::RenderContentEntry(const std::filesystem::path& path, const std::string& assetPath, ContentType contentType) {

        bool isDirectory = contentType == ContentType::None;
        // Ignore 'invisible' directories
        if (isDirectory && assetPath.at(0) == '.')
            return;

        auto filename = Common::Path::GetFileName(assetPath);

        ImVec2 buttonSize = ImVec2(iconSize, iconSize);

        ImGui::BeginGroup();

        ImGui::PushID(path.c_str());

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        Texture::Texture2D iconTexture;
        if (isDirectory)
            iconTexture = Singletons::icons->Get(IconType::Folder);
        else
            iconTexture = GetIcon(contentType);
        auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(iconTexture);

        std::string fileType = Common::Path::GetFileType(assetPath);
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

        // Assign default value, which is not valid if this dir entry is a directory
        auto type = ContentType::Audio;
        if (!isDirectory)
            type = Content::contentTypeMapping.at(fileType);

        auto assetRelativePath = Common::Path::Normalize(assetPath);
        auto buttonFlags = isDirectory || type == ContentType::Scene ? ImGuiButtonFlags_PressedOnDoubleClick : 0;
        if (ImGui::ImageButtonEx(ImGui::GetID("ImageButton"), set, buttonSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
            ImVec4(0.0f, 0.0f, 0.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), buttonFlags)) {
            if (isDirectory)
                nextDirectory = path.string();
            else if (type == ContentType::Scene)
                FileImporter::ImportFile<Scene::Scene>(assetRelativePath);
        }

        if (!isDirectory && ImGui::BeginDragDropSource()) {
            // Size doesn't count the termination character, so add +1
            ImGui::SetDragDropPayload("ContentBrowserResource", assetRelativePath.data(), assetRelativePath.size() + 1);
            ImGui::Text("Drag to entity component");

            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginPopupContextItem()) {
            // Do a direct import here without relying on the file importer
            if (type == ContentType::MeshSource && ImGui::MenuItem("Import as scene")) {
                PopupPanels::filename = assetRelativePath;
                PopupPanels::isImportScenePopupVisible = true;
            }

            // We shouldn't allow the user to delete the root entity
            if (ImGui::MenuItem("Open externally"))
                OpenExternally(std::filesystem::absolute(path).string(), isDirectory);

            if (ImGui::MenuItem("Rename")) {
                renamePopupVisible = true;
                auto dirEntryFilename = path.filename();
                renameString = dirEntryFilename.replace_extension("").string();
                renamePath = path;
            }

            if (ImGui::MenuItem("Delete"))
                std::filesystem::remove(path);

            ImGui::EndPopup();
        }

        ImGui::PopStyleColor();

        auto offset = 0.0f;

        auto textSize = ImGui::CalcTextSize(filename.c_str());
        if (textSize.x < iconSize + padding)
            offset = (iconSize + padding - textSize.x) / 2.0f;

        auto cursorX = ImGui::GetCursorPosX();
        ImGui::SetCursorPosX(cursorX + offset);
        ImGui::TextWrapped("%s", filename.c_str());

        ImGui::PopID();

        ImGui::EndGroup();

        ImGui::NextColumn();

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
        default: return icons->Get(IconType::Document);
        }

    }

    void ContentBrowserWindow::UpdateFilteredAndSortedDirEntries() {

        ContentType filterFileType = ContentType::None;
        if (selectedFilter >= 0)
            filterFileType = static_cast<ContentType>(selectedFilter);

        auto contentDirectory = ContentDiscovery::GetDirectory(currentDirectory);
        if (!contentDirectory)
            return;

        std::string searchQuery = assetSearch;
        if (!searchQuery.empty()) {
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
        }

        const auto& settings = Singletons::config->contentBrowserSettings;

        std::vector<Content> discoverdFiles;
        bool recursively = searchQuery.empty() ? settings.filterRecursively && filterFileType != ContentType::None : settings.searchRecursively;
        SearchDirectory(contentDirectory, discoverdFiles, filterFileType, searchQuery, recursively);

        std::sort(discoverdFiles.begin(), discoverdFiles.end(), [](const auto& file0, const auto& file1) {
            return file0.name < file1.name;
        });

        if (assetSearch.empty() && !settings.filterRecursively) {
            directories = contentDirectory->directories;
            files = discoverdFiles;
        }
        else {
            files = discoverdFiles;
            directories.clear();
        }

    }

    void ContentBrowserWindow::SearchDirectory(const Ref<ContentDirectory>& directory, std::vector<Content>& contentFiles, 
            const ContentType contentType, const std::string& searchQuery, bool recursively) {

        for (const auto& file : directory->files) {
            if (file.type != contentType && contentType != ContentType::None) {
                continue;
            }
            
            if (!searchQuery.empty() && file.name.find(searchQuery) == std::string::npos) {
                continue;
            }

            contentFiles.push_back(file);
        }

        if (recursively) {
            for (const auto& childDirectory : directory->directories) {
                SearchDirectory(childDirectory, contentFiles, contentType, searchQuery, true);
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

    bool ContentBrowserWindow::TextInputPopup(const char* name, bool& isVisible, std::string& input) {

        if (!isVisible)
            return false;

        PopupPanels::SetupPopupSize(0.4f, 0.1f);

        bool popupNew = false;

        if (!ImGui::IsPopupOpen(name)) {
            popupNew = true;
            ImGui::OpenPopup(name);
        }

        bool success = false;

        if (ImGui::BeginPopupModal(name, nullptr, ImGuiWindowFlags_NoResize)) {

            if (popupNew)
                ImGui::SetKeyboardFocusHere();

            ImGui::InputText("New name", &input);

            if (ImGui::Button("Cancel")) {
                isVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Ok") || ImGui::IsKeyReleased(ImGuiKey_Enter)) {
                success = true;
                isVisible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();

        }

        return success;

    }

}