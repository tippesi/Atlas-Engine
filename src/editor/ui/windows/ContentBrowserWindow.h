#pragma once

#include "Window.h"
#include "Singletons.h"
#include "Icons.h"
#include "ContentDiscovery.h"
#include "resource/ResourceManager.h"
#include "common/Path.h"
#include "ImguiExtension/ImguiWrapper.h"

#include <cctype>
#include <filesystem>
#include <tuple>

namespace Atlas::Editor::UI {

    class ContentBrowserWindow : public Window {

    public:
        explicit ContentBrowserWindow(bool show);

        void Render();

    private:
        void RenderDirectoryControl();

        void RenderDirectoryContent();

        void RenderContentEntry(const std::filesystem::path& path, const std::string& assetPath, ContentType contentType);

        bool IsValidFileType(const std::string& filename);

        Texture::Texture2D& GetIcon(const ContentType contentType);

        void UpdateFilteredAndSortedDirEntries();

        void OpenExternally(const std::string& path, bool isDirectory);

        bool TextInputPopup(const char* name, bool& isVisible, std::string& input);

        int selectedFilter = -1;

        std::string currentDirectory = Loader::AssetLoader::GetAssetDirectory();
        std::string nextDirectory;
        std::string assetSearch;

        std::string renameString;
        std::filesystem::path renamePath;
        bool renamePopupVisible = false;

        std::vector<Ref<ContentDirectory>> directories;
        std::vector<Content> files;

        const float padding = 8.0f;
        const float iconSize = 64.f;
        const float itemSize = iconSize + 2.0f * padding;

    };

}