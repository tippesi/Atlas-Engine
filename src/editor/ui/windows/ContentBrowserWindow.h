#pragma once

#include "Window.h"
#include "Singletons.h"
#include "Icons.h"
#include "resource/ResourceManager.h"
#include "common/Path.h"
#include "ImguiExtension/ImguiWrapper.h"

#include <cctype>
#include <filesystem>

namespace Atlas::Editor::UI {

    class ContentBrowserWindow : public Window {

    public:
        explicit ContentBrowserWindow(bool show);

        void Render();

    private:
        void RenderDirectoryControl();

        void RenderDirectoryContent();

        bool IsValidFileType(const std::string& filename);

        Texture::Texture2D& GetIcon(const std::filesystem::directory_entry& dirEntry);

        std::vector<std::filesystem::directory_entry> GetFilteredAndSortedDirEntries();

        void OpenExternally(const std::string& path, bool isDirectory);

        bool TextInputPopup(const char* name, bool& isVisible, std::string& input);

        int selectedFilter = -1;

        std::string currentDirectory = Loader::AssetLoader::GetAssetDirectory();
        std::string assetSearch;

        std::string renameString;
        std::filesystem::directory_entry renameDirEntry;
        bool renamePopupVisible = false;

    };

}