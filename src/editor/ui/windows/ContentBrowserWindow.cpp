#include "ContentBrowserWindow.h"
#include "FileImporter.h"

#include "mesh/Mesh.h"
#include "scene/Scene.h"
#include "audio/AudioData.h"

#include <filesystem>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#ifdef AE_OS_WINDOWS
#include <Windows.h>
#include <shellapi.h>
#endif

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

        const char *items[] = {"Audio", "Mesh", "Terrain", "Scene", "Script", "Font"};
        static int currentSelection = -1;
        for (int i = 0; i < IM_ARRAYSIZE(items); i++) {
            bool isSelected = currentSelection == i;
            ImGui::Selectable(items[i], &isSelected, ImGuiSelectableFlags_SpanAvailWidth);
            if (isSelected) {
                currentSelection = i;
            }
            else if (!isSelected && currentSelection == i) {
                currentSelection = -1;
            }
        }

        ImGui::End();

        ImGui::Begin("ResourceTypeOverview", nullptr);

        RenderDirectoryControl();

        ImGui::Separator();

        RenderDirectoryContent();

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

        // Use some less padding due to font scale shift
        ImGui::SetCursorPosY(6.0f);

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
            if (!isDirectory && !IsValidFileType(path))
                continue;

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

            auto buttonFlags = isDirectory ? ImGuiButtonFlags_PressedOnDoubleClick : 0;            
            if (ImGui::ImageButtonEx(ImGui::GetID("ImageButton"), set, buttonSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                ImVec4(0.0f, 0.0f, 0.0f, 0.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f), buttonFlags)) {
                if (isDirectory) 
                    nextDirectory = path;
            }

            if (!isDirectory && ImGui::BeginDragDropSource()) {
                auto assetRelativePath = Common::Path::GetRelative(assetDirectory, path);
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
        default: return icons->Get(IconType::Document);
        }

    }

    std::vector<std::filesystem::directory_entry> ContentBrowserWindow::GetFilteredAndSortedDirEntries() {

        using dir_entry = std::filesystem::directory_entry;

        std::vector<dir_entry> entries;
        if (assetSearch.empty()) {
            for (const auto& dirEntry : std::filesystem::directory_iterator(currentDirectory))
                entries.push_back(dirEntry);
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

                if (filename.find(searchQuery) != std::string::npos)
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
        ShellExecute(NULL, "open", path.c_str(), NULL, NULL, SW_SHOWDEFAULT);
#endif
#if defined(AE_OS_LINUX) || defined(AE_OS_MACOS)
        auto command = "xdg-open " + path;
        system(command.c_str());
#endif
        
    }

}