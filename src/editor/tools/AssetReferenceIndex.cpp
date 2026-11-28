#include "AssetReferenceIndex.h"
#include "UriHelper.h"
#include "../ContentDiscovery.h"
#include "../Notifications.h"

#include "Log.h"
#include "common/Path.h"
#include "common/SerializationHelper.h"
#include "loader/AssetLoader.h"
#include "loader/AssetReferences.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <string_view>

namespace Atlas::Editor {

    std::unordered_map<std::string, AssetReferenceIndex::File> AssetReferenceIndex::files;
    std::vector<std::pair<std::string, AssetReferenceIndex::File>> AssetReferenceIndex::changedFiles;
    AssetReferenceIndex::AssetSet AssetReferenceIndex::knownAssets = std::make_shared<std::unordered_set<std::string>>();
    std::atomic<size_t> AssetReferenceIndex::processedCount = 0;
    bool AssetReferenceIndex::requested = false;
    bool AssetReferenceIndex::indexing = false;
    size_t AssetReferenceIndex::discovery = 0;
    JobGroup AssetReferenceIndex::indexGroup { "Asset references", JobPriority::Medium };
    JobGroup AssetReferenceIndex::renameGroup { "Rename references", JobPriority::Medium };

    void AssetReferenceIndex::Request() {

        requested = true;
        discovery = 0;

    }

    void AssetReferenceIndex::Update() {

        if (indexing && indexGroup.HasFinished()) {
            for (auto& [path, file] : changedFiles)
                files[path] = std::move(file);
            changedFiles.clear();
            indexing = false;
        }

        // Files which are still rewritten would be outdated right away
        if (!requested || indexing || !renameGroup.HasFinished())
            return;

        if (!discovery)
            discovery = ContentDiscovery::Execute();
        if (!ContentDiscovery::HasFinished(discovery))
            return;

        requested = false;
        StartIndexing();

    }

    void AssetReferenceIndex::StartIndexing() {

        auto assets = std::make_shared<std::unordered_set<std::string>>();
        changedFiles.clear();
        for (const auto& content : ContentDiscovery::GetAllContent()) {
            auto lowerPath = Common::Path::ToLower(content.assetPath);
            assets->insert(lowerPath);

            auto known = files.find(content.assetPath);
            if (HasReferences(content) && (known == files.end() || known->second.lastWriteTime != content.lastWriteTime ||
                known->second.size != content.size))
                changedFiles.push_back({ content.assetPath, { .lastWriteTime = content.lastWriteTime, .size = content.size,
                    .lowerPath = lowerPath } });
        }
        knownAssets = assets;
        std::erase_if(files, [&](const auto& file) { return !assets->contains(file.second.lowerPath); });

        processedCount = 0;
        indexing = true;
        // Few jobs, the disk limits them anyway
        JobSystem::ParallelFor(indexGroup, int32_t(changedFiles.size()), 4, [assets](JobData&, int32_t idx) {
            auto& [path, file] = changedFiles[idx];
            file.references = FindReferences(path, ReadFile(path, GetSearchedSize(path)), assets);
            for (auto& reference : file.references)
                reference.path = Common::Path::ToLower(reference.path);

            std::sort(file.references.begin(), file.references.end());
            file.references.erase(std::unique(file.references.begin(), file.references.end()), file.references.end());
            processedCount++;
            });

    }

    float AssetReferenceIndex::GetProgress() {

        if (requested)
            return 0.0f;
        return !indexing || changedFiles.empty() ? 1.0f : float(processedCount) / float(changedFiles.size());

    }

    std::vector<std::string> AssetReferenceIndex::FindUsers(const std::string& path, bool renamed) {

        auto lowerPath = Common::Path::ToLower(path);
        auto directory = lowerPath + "/";

        std::vector<std::string> users;
        for (const auto& [owner, file] : files) {
            auto isInside = file.lowerPath.starts_with(directory);
            if (isInside && !renamed)
                continue;

            // The references are sorted, the files of a directory follow each other
            auto uses = [&](const std::string& beginning, bool isDirectory) {
                auto reference = std::lower_bound(file.references.begin(), file.references.end(), Reference { .path = beginning });
                for (; reference != file.references.end(); reference++) {
                    if (isDirectory ? !reference->path.starts_with(beginning) : reference->path != beginning)
                        return false;
                    if (!isInside || !reference->relative)
                        return true;
                }
                return false;
                };
            if (uses(lowerPath, false) || uses(directory, true))
                users.push_back(owner);
        }

        std::sort(users.begin(), users.end());
        return users;

    }

    bool AssetReferenceIndex::IsAsset(const std::string& path) {

        return knownAssets->contains(Common::Path::ToLower(path));

    }

    void AssetReferenceIndex::RenameInFiles(const std::vector<std::string>& files, const std::string& oldPath,
        const std::string& newPath) {

        JobSystem::Execute(renameGroup, [files, assets = knownAssets, oldPath, newPath](JobData&) {
            size_t changedCount = 0, failedCount = 0;
            for (const auto& file : files) {
                // Files in the renamed directory moved with it
                auto path = Common::Path::Rename(file, oldPath, newPath);
                auto data = ReadFile(path, std::numeric_limits<size_t>::max());
                auto renamedData = Rename(file, data, assets, oldPath, newPath);
                if (renamedData == data)
                    continue;
                if (Loader::AssetLoader::WriteFileAtomically(path, renamedData.data(), renamedData.size())) {
                    changedCount++;
                    continue;
                }
                failedCount++;
                Log::Warning("The references of " + path + " to " + oldPath + " couldn't be changed to " + newPath);
            }

            if (changedCount > 0)
                Notifications::Push({ .message = "Updated " + std::to_string(changedCount) + (changedCount == 1 ? " file" : " files") });
            if (failedCount > 0)
                Notifications::Push({ .message = "Couldn't update " + std::to_string(failedCount) +
                    (failedCount == 1 ? " file" : " files") + ", the log lists them", .type = NotificationType::Warning });
            ContentDiscovery::Execute();
            });

    }

    std::string AssetReferenceIndex::RenameInContent(const std::string& file, const std::string& content,
        const std::string& oldPath, const std::string& newPath) {

        auto data = Rename(file, std::vector<char>(content.begin(), content.end()), knownAssets, oldPath, newPath);
        return std::string(data.begin(), data.end());

    }

    void AssetReferenceIndex::Shutdown() {

        JobSystem::Wait(indexGroup);
        JobSystem::Wait(renameGroup);
        files.clear();
        changedFiles.clear();
        knownAssets = std::make_shared<std::unordered_set<std::string>>();
        requested = false;
        indexing = false;

    }

    bool AssetReferenceIndex::HasReferences(const Content& content) {

        switch (content.type) {
        case ContentType::Mesh:
        case ContentType::Material:
        case ContentType::MaterialGraph:
        case ContentType::MaterialSubgraph:
        case ContentType::Terrain:
        case ContentType::Scene:
        case ContentType::Script:
        case ContentType::Prefab: return true;
        case ContentType::MeshSource: return Common::Path::GetFileType(content.name) == "gltf";
        default: return false;
        }

    }

    AssetReferenceIndex::Format AssetReferenceIndex::GetFormat(const std::string& path, std::span<const char> data) {

        if (path.ends_with(".lua") || path.ends_with(".aeterrain") || path.ends_with(".gltf") || data.size() < 2)
            return Format::Text;

        // Like the scene loader: MessagePack never starts with a bracket, BJData continues with the length of a key
        if (data[0] != '{')
            return Format::MessagePack;
        return data[1] == '"' || data[1] == '}' || std::isspace(uint8_t(data[1])) ? Format::Text : Format::BJData;

    }

    std::vector<char> AssetReferenceIndex::ReadFile(const std::string& path, size_t maxSize) {

        auto stream = Loader::AssetLoader::ReadFile(path, std::ios::in | std::ios::binary);
        if (!stream.is_open())
            return {};

        stream.seekg(0, std::ios::end);
        std::vector<char> data(std::min(size_t(stream.tellg()), maxSize));
        stream.seekg(0, std::ios::beg);
        stream.read(data.data(), std::streamsize(data.size()));
        return data;

    }

    size_t AssetReferenceIndex::GetSearchedSize(const std::string& path) {

        return path.ends_with(".aeterrain") ? maxTerrainHeaderSize : std::numeric_limits<size_t>::max();

    }

    std::vector<AssetReferenceIndex::Reference> AssetReferenceIndex::FindReferences(const std::string& file,
        std::span<const char> data, const AssetSet& assets) {

        std::vector<Reference> references;

        auto format = GetFormat(file, data);
        if (format == Format::BJData) {
            auto j = json::from_bjdata(data, true, false);
            for (const auto& [_, value] : j.flatten().items()) {
                Reference reference;
                if (value.is_string() && Resolve(file, value.get<std::string>(), assets, reference))
                    references.push_back(reference);
            }
            return references;
        }

        auto foundReferences = format == Format::Text ? FindTextReferences(file, data, assets) :
            FindMessagePackReferences(file, data, assets);
        for (const auto& found : foundReferences)
            references.push_back(found.reference);
        return references;

    }

    std::vector<AssetReferenceIndex::FoundReference> AssetReferenceIndex::FindTextReferences(const std::string& file,
        std::span<const char> data, const AssetSet& assets) {

        // Everything that isn't ASCII is part of an UTF-8 character
        auto isPathCharacter = [](char c) {
            return std::isalnum(uint8_t(c)) || uint8_t(c) >= 0x80 || (c != 0 && std::strchr("_-./\\ ()+&,@#~[]!=%'", c) != nullptr);
            };

        std::vector<FoundReference> references;
        auto begin = data.data();
        auto end = begin + data.size();
        auto dot = begin;
        while (auto pathEnd = FindExtension(dot, end)) {
            auto start = dot;
            while (start > begin && size_t(dot - start) < maxPathLength && isPathCharacter(*(start - 1)))
                start--;

            // Code in front of the path can look like a part of it
            for (auto candidate = start; candidate < dot; candidate++) {
                if (candidate != start && !std::string_view(" =(,'").contains(*(candidate - 1)))
                    continue;

                FoundReference found { .offset = size_t(candidate - begin), .length = size_t(pathEnd - candidate) };
                if (!Resolve(file, std::string(candidate, pathEnd), assets, found.reference))
                    continue;
                references.push_back(found);
                break;
            }

            dot = pathEnd;
        }

        return references;

    }

    std::vector<AssetReferenceIndex::FoundReference> AssetReferenceIndex::FindMessagePackReferences(const std::string& file,
        std::span<const char> data, const AssetSet& assets) {

        std::vector<FoundReference> references;
        auto begin = data.data();
        auto end = begin + data.size();

        auto getHeaderSize = [&](const char* start, size_t length) -> size_t {
            auto byte = [&](ptrdiff_t offset) { return start - begin >= offset ? uint8_t(start[-offset]) : uint8_t(0); };
            if (length < 32)
                return byte(1) == (0xa0 | length) ? 1 : 0;
            if (length < 256)
                return byte(2) == 0xd9 && byte(1) == length ? 2 : 0;
            return byte(3) == 0xda && byte(2) == length >> 8 && byte(1) == (length & 0xff) ? 3 : 0;
            };

        auto dot = begin;
        while (auto pathEnd = FindExtension(dot, end)) {
            for (auto start = dot; start > begin && size_t(pathEnd - start) <= maxPathLength; start--) {
                auto headerSize = getHeaderSize(start, size_t(pathEnd - start));
                if (!headerSize)
                    continue;

                FoundReference found { .offset = size_t(start - begin) - headerSize, .length = size_t(pathEnd - start) + headerSize };
                if (!Resolve(file, std::string(start, pathEnd), assets, found.reference))
                    continue;
                references.push_back(found);
                break;
            }

            dot = pathEnd;
        }

        return references;

    }

    const char* AssetReferenceIndex::FindExtension(const char*& dot, const char* end) {

        for (; dot < end; dot++) {
            dot = static_cast<const char*>(std::memchr(dot, '.', size_t(end - dot)));
            if (!dot)
                return nullptr;

            auto extensionEnd = dot + 1;
            while (extensionEnd < end && extensionEnd - dot < 16 && std::isalnum(uint8_t(*extensionEnd)))
                extensionEnd++;

            // Extensions can follow each other, e.g. of the impostor next to its mesh
            auto isFollowed = end - extensionEnd > 1 && *extensionEnd == '.' && std::isalnum(uint8_t(extensionEnd[1]));
            if (!isFollowed && Content::contentTypeMapping.contains(Common::Path::ToLower(std::string(dot + 1, extensionEnd))))
                return extensionEnd;
        }

        return nullptr;

    }

    bool AssetReferenceIndex::Resolve(const std::string& file, std::string text, const AssetSet& assets, Reference& reference) {

        std::replace(text.begin(), text.end(), '\\', '/');

        if (file.ends_with(".gltf")) {
            auto path = std::filesystem::path(file).parent_path() / UriHelper::Decode(text);
            reference = { .path = path.lexically_normal().generic_string(), .relative = true };
        }
        else {
            reference = { .path = Loader::AssetReferences::ResolveReference(file, text),
                .relative = text.starts_with("./") || text.starts_with("../") };
        }

        auto lowerPath = Common::Path::ToLower(reference.path);
        return assets->contains(lowerPath) && lowerPath != Common::Path::ToLower(file);

    }

    std::string AssetReferenceIndex::GetRenamedReference(const std::string& file, const Reference& reference,
        const std::string& oldPath, const std::string& newPath) {

        auto path = Common::Path::Rename(reference.path, oldPath, newPath);
        if (path == reference.path)
            return {};
        if (!reference.relative)
            return path;

        auto newFile = Common::Path::Rename(file, oldPath, newPath);
        if (!file.ends_with(".gltf"))
            return Loader::AssetReferences::MakeReference(newFile, path);

        auto directory = std::filesystem::path(newFile).parent_path();
        return UriHelper::Encode(std::filesystem::path(path).lexically_relative(directory).generic_string());

    }

    std::vector<char> AssetReferenceIndex::Rename(const std::string& file, const std::vector<char>& data, const AssetSet& assets,
        const std::string& oldPath, const std::string& newPath) {

        auto format = GetFormat(file, data);
        if (format == Format::BJData) {
            auto j = json::from_bjdata(data, true, false);
            auto changed = false;
            for (const auto& [pointer, value] : j.flatten().items()) {
                Reference reference;
                if (!value.is_string() || !Resolve(file, value.get<std::string>(), assets, reference))
                    continue;
                auto renamedReference = GetRenamedReference(file, reference, oldPath, newPath);
                if (renamedReference.empty())
                    continue;
                j[json::json_pointer(pointer)] = renamedReference;
                changed = true;
            }
            if (!changed)
                return data;
            std::vector<char> renamedData;
            json::to_bjdata(j, renamedData);
            return renamedData;
        }

        auto searchedData = std::span<const char>(data).first(std::min(data.size(), GetSearchedSize(file)));
        auto references = format == Format::Text ? FindTextReferences(file, searchedData, assets) :
            FindMessagePackReferences(file, searchedData, assets);

        std::vector<char> renamedData;
        size_t copiedSize = 0;
        for (const auto& found : references) {
            auto renamedReference = GetRenamedReference(file, found.reference, oldPath, newPath);
            if (renamedReference.empty())
                continue;

            std::vector<char> content(renamedReference.begin(), renamedReference.end());
            if (format == Format::MessagePack) {
                content.clear();
                json::to_msgpack(json(renamedReference), content);
            }
            renamedData.insert(renamedData.end(), data.begin() + copiedSize, data.begin() + found.offset);
            renamedData.insert(renamedData.end(), content.begin(), content.end());
            copiedSize = found.offset + found.length;
        }
        renamedData.insert(renamedData.end(), data.begin() + copiedSize, data.end());
        return renamedData;

    }

}
