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

        ~ContentBrowserWindow() { JobSystem::Wait(searchAndFilterJob); }

        void Update();

        void Render();

        std::string currentDirectory = Loader::AssetLoader::GetAssetDirectory();
        
        static inline std::string contentToShowPath = "";

    private:
        struct ContentCopy {
            std::vector<std::string> paths;
        };

        enum EditingType {
            Create = 0,
            Rename
        };

        void RenderDirectoryControl();

        void RenderDirectoryContent();

        void RenderContentEntry(const std::filesystem::path& path, const std::string& assetPath, 
            ContentType contentType, int32_t entryIdx, int32_t columnCount, float columnSize, float& columnHeight);

        bool IsValidFileType(const std::string& filename);

        Texture::Texture2D& GetIcon(const ContentType contentType);

        void UpdateFilteredAndSortedDirEntries();

        void SearchDirectory(const Ref<ContentDirectory>& directory, std::vector<Content>& contentFiles, 
            const ContentType contentType, const std::string& searchQuery, bool recursively);

        void OpenExternally(const std::string& path, bool isDirectory);

        std::vector<std::string> GetSelectedPaths();

        void ApplyEdit(ContentType type);

        int selectedFilter = -1;

        std::string nextDirectory;
        std::string assetSearch;
        std::string hightlightPath;

        std::vector<Ref<ContentDirectory>> directories;
        std::vector<Content> files;

        JobGroup searchAndFilterJob{ JobPriority::Medium };

        ImGuiSelectionBasicStorage selectionStorage;

        bool isEditing = false;
        bool editingChanged = false;
        EditingType editingType = EditingType::Create;
        std::string editingString;
        std::filesystem::path editingPath;

        const float padding = 8.0f;
        const float iconSize = 64.f;
        const float itemSize = iconSize + 2.0f * padding;

    };

}