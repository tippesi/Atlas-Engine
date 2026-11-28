#pragma once

#include "../Content.h"
#include "jobsystem/JobSystem.h"

#include <atomic>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Atlas::Editor {

    // Which files of the asset directory use which other ones. References are found as paths in the content of the
    // files, so scripts and the resource tables of scenes are covered like materials. Names scripts build at runtime and
    // modules without their extension are not
    class AssetReferenceIndex {

    public:
        // Lower case paths
        using AssetSet = std::shared_ptr<const std::unordered_set<std::string>>;

        // Finds the content of the asset directory again and reads the files which changed, both in the background
        static void Request();

        // Called each frame
        static void Update();

        static bool IsReady() { return !requested && !indexing; }

        // From 0 to 1
        static float GetProgress();

        // The files which use a file, or a file of a directory. The ones in the directory itself only matter if it's
        // renamed, and only if their references don't start at their own place
        static std::vector<std::string> FindUsers(const std::string& path, bool renamed = false);

        // Whether there was such a file when the references were requested
        static bool IsAsset(const std::string& path);

        // Writes the new path into the files which use the old one, after it was renamed
        static void RenameInFiles(const std::vector<std::string>& files, const std::string& oldPath, const std::string& newPath);

        // The content of a file with the new path, e.g. of a script which is open. The file has its path from before
        static std::string RenameInContent(const std::string& file, const std::string& content,
            const std::string& oldPath, const std::string& newPath);

        static void Shutdown();

    private:
        enum class Format {
            Text,
            MessagePack,
            BJData
        };

        struct Reference {
            // In the asset directory
            std::string path;
            // Starts at the directory of the file which has it
            bool relative = false;

            auto operator<=>(const Reference& that) const = default;
        };

        struct FoundReference {
            size_t offset = 0;
            size_t length = 0;
            Reference reference;
        };

        struct File {
            int64_t lastWriteTime = 0;
            uintmax_t size = 0;
            std::string lowerPath;
            // Lower case paths, sorted
            std::vector<Reference> references;
        };

        static void StartIndexing();

        static bool HasReferences(const Content& content);

        static Format GetFormat(const std::string& path, std::span<const char> data);

        static std::vector<char> ReadFile(const std::string& path, size_t maxSize);

        // How much of the content of a file can have references
        static size_t GetSearchedSize(const std::string& path);

        static std::vector<Reference> FindReferences(const std::string& file, std::span<const char> data, const AssetSet& assets);

        static std::vector<FoundReference> FindTextReferences(const std::string& file, std::span<const char> data, const AssetSet& assets);

        // Including the length in front of the strings
        static std::vector<FoundReference> FindMessagePackReferences(const std::string& file, std::span<const char> data,
            const AssetSet& assets);

        // Moves to the dot of the next extension of some content and returns the end of it, nullptr without one
        static const char* FindExtension(const char*& dot, const char* end);

        // Whether the text is the path of an asset
        static bool Resolve(const std::string& file, std::string text, const AssetSet& assets, Reference& reference);

        // Written like before, with the path the asset has now. Empty if it wasn't renamed
        static std::string GetRenamedReference(const std::string& file, const Reference& reference,
            const std::string& oldPath, const std::string& newPath);

        static std::vector<char> Rename(const std::string& file, const std::vector<char>& data, const AssetSet& assets,
            const std::string& oldPath, const std::string& newPath);

        // Terrains are large, their materials stand in front of the heights
        static constexpr size_t maxTerrainHeaderSize = 1 << 20;
        static constexpr size_t maxPathLength = 260;

        static std::unordered_map<std::string, File> files;
        static std::vector<std::pair<std::string, File>> changedFiles;
        static AssetSet knownAssets;

        static std::atomic<size_t> processedCount;
        static bool requested;
        static bool indexing;
        // The discovery which finds the changes made before the request, zero until it was started
        static size_t discovery;

        static JobGroup indexGroup;
        static JobGroup renameGroup;

    };

}
