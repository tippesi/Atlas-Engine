#include "ContentBrowserWindow.h"
#include "FileImporter.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"

#include <filesystem>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

/*
#ifdef AE_OS_WINDOWS
#include <Windows.h>
#include <shellapi.h>
#endif
*/
namespace Atlas::Editor::UI {

    ContentBrowserWindow::ContentBrowserWindow(bool show) : Window("Object browser", show) {



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

            ImGuiDockNode *leftNode = ImGui::DockBuilderGetNode(dockIdLeft);
            ImGuiDockNode *rightNode = ImGui::DockBuilderGetNode(dockIdRight);
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
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Framed;

        const char *items[] = {"Audio", "Mesh", "Terrain", "Scene", "Script", "Font", "Prefab"};
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
                        [&](const ResourceHandle<Scene::Scene>& item) -> bool { return item->name == scene->name; });
                    auto sceneHandle = *iter;

                    auto nameComponent = entity.TryGetComponent<NameComponent>();
                    auto entityName = nameComponent ? nameComponent->name : "Entity " + std::to_string(entity);

                    auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();
                    auto assetPath = Common::Path::GetRelative(assetDirectory, currentDirectory);
                    auto entityPath = assetPath + entityName + ".aeprefab";
                    Scene::SceneSerializer::SerializePrefab(sceneHandle.Get(), entity, entityPath);
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

    }

    void ContentBrowserWindow::RenderDirectoryContent() {

        const float padding = 8.0f;
        const float iconSize = 64.f;

        const float itemSize = iconSize + 2.0f * padding;

        float totalWidth = ImGui::GetContentRegionAvail().x;
        auto columnItemCount = int32_t(totalWidth / itemSize);
        columnItemCount = columnItemCount <= 0 ? 1 : columnItemCount;

        ImGui::Columns(columnItemCount, nullptr, false);

        auto entries = GetFilteredAndSortedDirEntries();

        auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();

        std::string nextDirectory;
        for (const auto& dirEntry : entries) {

            if (!dirEntry.exists())
                continue;

            auto isDirectory = dirEntry.is_directory();

            auto path = dirEntry.path().string();
            auto assetPath = Common::Path::GetRelative(assetDirectory, path);

            // Ignore 'invisible' directories
            if (isDirectory && assetPath.at(0) == '.')
                continue;

            auto filename = Common::Path::GetFileName(path);

            ImVec2 buttonSize = ImVec2(iconSize, iconSize);

            ImGui::BeginGroup();

            ImGui::PushID(path.c_str());

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

            auto& icon = GetIcon(dirEntry);
            auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(icon);

            std::string fileType = Common::Path::GetFileType(path);
            std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

            // Assign default value, which is not valid if this dir entry is a directory
            auto type = FileType::Audio;
            if (!dirEntry.is_directory())
                type = FileImporter::fileTypeMapping.at(fileType);

            auto assetRelativePath = Common::Path::GetRelative(assetDirectory, path);
            assetRelativePath = Common::Path::Normalize(assetRelativePath);
            if (assetRelativePath.starts_with('/'))
                assetRelativePath.erase(assetRelativePath.begin());

            auto buttonFlags = isDirectory || type == FileType::Scene ? ImGuiButtonFlags_PressedOnDoubleClick : 0;            
            if (ImGui::ImageButtonEx(ImGui::GetID("ImageButton"), set, buttonSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                ImVec4(0.0f, 0.0f, 0.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), buttonFlags)) {
                if (isDirectory) 
                    nextDirectory = path;
                else if (type == FileType::Scene)
                    FileImporter::ImportFile<Scene::Scene>(assetRelativePath);
            }

            if (!isDirectory && ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ContentBrowserResource", assetRelativePath.c_str(), assetRelativePath.size());
                ImGui::Text("Drag to entity component");

                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginPopupContextItem()) {
                // We shouldn't allow the user to delete the root entity
                if (ImGui::MenuItem("Open externally"))
                    OpenExternally(std::filesystem::absolute(dirEntry.path()).string(), isDirectory);

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

        if (!nextDirectory.empty()) {
            currentDirectory = nextDirectory;
        }

    }

    bool ContentBrowserWindow::IsValidFileType(const std::string& filename) {

        std::string fileType = Common::Path::GetFileType(filename);
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

        return FileImporter::fileTypeMapping.contains(fileType);

    }

    Texture::Texture2D& ContentBrowserWindow::GetIcon(const std::filesystem::directory_entry& dirEntry) {
        
        auto& icons = Singletons::icons;

        if (dirEntry.is_directory())
            return icons->Get(IconType::Folder);

        auto path = dirEntry.path().string();
        std::string fileType = Common::Path::GetFileType(path);
        std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

        auto type = FileImporter::fileTypeMapping.at(fileType);

        switch (type) {
        case FileType::Audio: return icons->Get(IconType::Audio);
        case FileType::Mesh: return icons->Get(IconType::Mesh);
        case FileType::Scene: return icons->Get(IconType::Scene);
        case FileType::Font: return icons->Get(IconType::Font);
        case FileType::Prefab: return icons->Get(IconType::Prefab);
        default: return icons->Get(IconType::Document);
        }

    }

    std::vector<std::filesystem::directory_entry> ContentBrowserWindow::GetFilteredAndSortedDirEntries() {

        using dir_entry = std::filesystem::directory_entry;

        FileType filterFileType;
        if (selectedFilter >= 0)
            filterFileType = static_cast<FileType>(selectedFilter);

        std::vector<dir_entry> entries;
        if (assetSearch.empty() && selectedFilter < 0) {
            for (const auto& dirEntry : std::filesystem::directory_iterator(currentDirectory)) {
                auto path = dirEntry.path().string();
                if (dirEntry.is_directory() || IsValidFileType(path))
                    entries.push_back(dirEntry);
            }
        }
        else {
            std::string searchQuery = assetSearch;
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
            for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(currentDirectory)) {
                if (dirEntry.is_directory())
                    continue;

                auto path = dirEntry.path().string();
                auto filename = Common::Path::GetFileName(path);
                std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

                if (!IsValidFileType(path))
                    continue;

                // Filter out by file type, if a filter is selected
                if (selectedFilter >= 0) {
                    std::string fileType = Common::Path::GetFileType(filename);
                    std::transform(fileType.begin(), fileType.end(), fileType.begin(), ::tolower);

                    auto type = FileImporter::fileTypeMapping.at(fileType);
                    if (type != filterFileType)
                        continue;
                }

                // Filter out by search query, if it is a valid (non-empty) query
                if (searchQuery.empty() || filename.find(searchQuery) != std::string::npos)
                    entries.push_back(dirEntry);
            }
        }

        // Sort for directories to be first and everything to be ordered alphabetically
        std::sort(entries.begin(), entries.end(),
            [](const dir_entry& entry0, const dir_entry& entry1) {
                if (entry0.is_directory() && entry1.is_directory()) {
                    return entry0.path() < entry1.path();
                }
                if (entry0.is_directory())
                    return true;
                if (entry1.is_directory())
                    return false;

                return entry0.path() < entry1.path();
            });

        return entries;

    }

    void ContentBrowserWindow::OpenExternally(const std::string& path, bool isDirectory) {

#ifdef AE_OS_WINDOWS
        // ShellExecute(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
#endif
#if defined(AE_OS_LINUX) || defined(AE_OS_MACOS)
        auto command = "open " + path;
        system(command.c_str());
#endif
        
    }

}