#pragma once

#include "DrawerWindow.h"
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

    class ContentBrowserWindow : public DrawerWindow {

    public:
        explicit ContentBrowserWindow(bool show);

        ~ContentBrowserWindow() { JobSystem::Wait(searchAndFilterJob); }

        void Update();

        void Render();

        typedef uint32_t ContentFilter;

		typedef enum ContentFilterBits {
			AudioBit = (1 << 0),
			MeshBit = (1 << 1),
			MeshSourceBit = (1 << 2),
			MaterialBit = (1 << 3),
			TerrainBit = (1 << 4),
			SceneBit = (1 << 5),
			ScriptBit = (1 << 6),
			FontBit = (1 << 7),
			PrefabBit = (1 << 8),
			TextureBit = (1 << 9),
			EnvTextureBit = (1 << 10),
			AnimationBit = (1 << 11),
			MaterialGraphBit = (1 << 12),
			AllBit = (1 << 13) - 1
		} ContentFilterBits;

        std::string currentDirectory = Loader::AssetLoader::GetDataDirectory();

        ContentFilter contentFilter = ContentFilterBits::AllBit;
        bool searchRecursively = true;
		bool filterRecursively = false;

        // Can be changed with Ctrl + mouse wheel
        float iconSize = 64.0f;
        static constexpr float minIconSize = 32.0f;
        static constexpr float maxIconSize = 160.0f;

        float folderPanelWidth = 220.0f;
        bool listView = false;
        
        static inline std::string contentToShowPath = "";
        // Mesh, material, prefab or animation the user wants to open in its editor
        static inline std::string contentToEditPath = "";

    private:
        struct ContentCopy {
            std::vector<std::string> paths;
        };

        enum EditingType {
            Create = 0,
            Rename
        };

        void RenderDirectoryControl();

        void RenderDirectoryContentControl();

        void RenderDirectoryContent();

        void RenderDirectoryEntry(const Ref<ContentDirectory>& directory);

        void RenderContentEntry(const std::filesystem::path& path, const std::string& assetPath, 
            ContentType contentType, uintmax_t fileSize, int32_t entryIdx, int32_t columnCount, float columnSize, float& columnHeight);

        void RenderTileLabel(const std::string& filename, float columnSize);

        void RenderListColumns(const std::string& filename, ContentType contentType, uintmax_t fileSize, float rowWidth);

        bool IsValidFileType(const std::string& filename);

        Texture::Texture2D& GetIcon(const ContentType contentType);

        const char* GetTypeName(ContentType contentType) const;

        void SearchDirectory(const Ref<ContentDirectory>& directory, std::vector<Content>& contentFiles,
            const std::string& searchQuery, ContentFilter filter, bool recursively);

        void OpenExternally(const std::string& path, bool isDirectory);

        std::vector<std::string> GetSelectedPaths();

        void ApplyEdit(ContentType type);

        void RenderDeleteConfirmation();

        // Files and folders the user chose to delete, removed once confirmed
        std::vector<std::string> pendingDeletePaths;
        bool pendingDeleteHasDirectory = false;
        bool openDeleteConfirmation = false;

        std::string nextDirectory;
        std::string assetSearch;
        std::string hightlightPath;

        std::vector<Ref<ContentDirectory>> directories;
        std::vector<Content> files;

        // Results of the search job, the window shows the last ones until the job has finished
        std::vector<Ref<ContentDirectory>> foundDirectories;
        std::vector<Content> foundFiles;
        bool hasFoundEntries = false;

        JobGroup searchAndFilterJob{ "Content search and filter", JobPriority::Medium };

        ImGuiSelectionBasicStorage selectionStorage;

        bool isEditing = false;
        bool editingChanged = false;
        EditingType editingType = EditingType::Create;
        std::string editingString;
        std::filesystem::path editingPath;

        const float padding = 8.0f;

    };

}