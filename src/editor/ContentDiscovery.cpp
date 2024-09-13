#include "ContentDiscovery.h"
#include "loader/AssetLoader.h"
#include "common/Path.h"
#include "Clock.h"
#include "Log.h"

#include <filesystem>
#include <algorithm>

namespace Atlas::Editor {

	Ref<ContentDiscovery::DiscoveredContent> ContentDiscovery::content = CreateRef<DiscoveredContent>();
	Ref<ContentDiscovery::DiscoveredContent> ContentDiscovery::nextContent = CreateRef<DiscoveredContent>();
	JobGroup ContentDiscovery::contentDiscoveryJob;

	const float ContentDiscovery::discoverFrequency = 3.0f;
	float ContentDiscovery::lastDiscoveryTime = -ContentDiscovery::discoverFrequency;

	const Ref<ContentDirectory> ContentDiscovery::GetContent() {

		return content->rootDirectory;

	}

	std::vector<Content> ContentDiscovery::GetContent(const ContentType type) {

		if (!content->contentTypeToContentMap.contains(type))
			return {};

		return content->contentTypeToContentMap.at(type);

	}

	std::vector<Content> ContentDiscovery::GetAllContent() {

		std::vector<Content> files;
		for (const auto& [type, typeFiles] : content->contentTypeToContentMap) {
			files.insert(files.end(), typeFiles.begin(), typeFiles.end());
		}

		return files;

	}

	const Ref<ContentDirectory> ContentDiscovery::GetDirectory(const std::string& path) {

		if (!content->contentDirectories.contains(path))
			return {};

		return content->contentDirectories.at(path);

	}

	void ContentDiscovery::Update() {

		bool canRediscover = (Clock::Get() - lastDiscoveryTime) >= discoverFrequency || content->contentDirectories.empty();

		if (contentDiscoveryJob.HasFinished() && canRediscover) {
			// Swap here to not immediately release the memory of content (causes stutter due to freeing memory)		
			std::swap(content, nextContent);

			lastDiscoveryTime = Clock::Get();
			JobSystem::Execute(contentDiscoveryJob, [&](JobData&) {
				// Now release the swapped memory here in the job async
				nextContent.reset();

				nextContent = PerformContentDiscovery();
			});


			return;
		}
	}

	Ref<ContentDiscovery::DiscoveredContent> ContentDiscovery::PerformContentDiscovery() {

		auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();

		auto rootDirectory = CreateRef<ContentDirectory>({
			.path = assetDirectory
		});
		auto result = CreateRef<DiscoveredContent>({
			.rootDirectory = rootDirectory,
		});
		result->contentDirectories[assetDirectory] = rootDirectory;

		DiscoverDirectory(rootDirectory, result);

		return result;

	}

	void ContentDiscovery::DiscoverDirectory(const Ref<ContentDirectory>& directory, const Ref<DiscoveredContent>& result) {

		auto assetDirectory = Loader::AssetLoader::GetAssetDirectory();

		for (const auto& dirEntry : std::filesystem::directory_iterator(directory->path)) {
			auto path = Common::Path::Normalize(dirEntry.path().string());
			auto assetPath = Common::Path::GetRelative(assetDirectory, path);
			if (assetPath.starts_with('/'))
				assetPath.erase(assetPath.begin());

			if (dirEntry.is_directory()) {
				auto dirname = Common::Path::GetFileName(dirEntry.path());
				std::transform(dirname.begin(), dirname.end(), dirname.begin(), ::tolower);

				auto childDirectory = CreateRef<ContentDirectory>({
						.name = dirname,
						.path = dirEntry.path(),
						.assetPath = assetPath
					});
				directory->directories.push_back(childDirectory);
				result->contentDirectories[childDirectory->path.string()] = childDirectory;
				DiscoverDirectory(childDirectory, result);
				continue;
			}

			auto filename = Common::Path::GetFileName(path);
			std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

			std::string fileType = Common::Path::GetFileType(filename);
			if (!Content::contentTypeMapping.contains(fileType))
				continue;

			auto contentType = Content::contentTypeMapping.at(fileType);
			directory->files.emplace_back(Content {
				.name = filename,
				.path = dirEntry.path().string(),
				.assetPath = assetPath,
				.type = contentType,
			});

			result->contentTypeToContentMap[contentType].push_back(directory->files.back());
		}

		// Sort for directories to be ordered alphabetically
		std::sort(directory->directories.begin(), directory->directories.end(),
			[](const Ref<ContentDirectory>& dir0, const Ref<ContentDirectory>& dir1) {
				return dir0->name < dir1->name;
			});

		// Sort for files to be ordered alphabetically
		std::sort(directory->files.begin(), directory->files.end(),
			[](const Content& file0, const Content& file1) {
				return file0.name < file1.name;
			});

	}

}