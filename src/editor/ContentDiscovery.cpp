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
	JobGroup ContentDiscovery::contentDiscoveryJob { "Content discovery" };
	bool ContentDiscovery::hasNextContent = false;

	std::atomic_bool ContentDiscovery::execute = false;
	std::atomic_size_t ContentDiscovery::startedCount = 0;
	size_t ContentDiscovery::finishedCount = 0;
	const float ContentDiscovery::discoverFrequency = 3.0f;
	float ContentDiscovery::lastDiscoveryTime = -ContentDiscovery::discoverFrequency;

	size_t ContentDiscovery::Execute() {

		execute = true;
		return startedCount + 1;

	}

	const Ref<ContentDirectory> ContentDiscovery::GetContent() {

		return content ? content->rootDirectory : nullptr;

	}

	std::vector<Content> ContentDiscovery::GetContent(const ContentType type) {

		if (!content || !content->contentTypeToContentMap.contains(type))
			return {};

		return content->contentTypeToContentMap.at(type);

	}

	std::vector<Content> ContentDiscovery::GetAllContent() {

		if (!content)
			return {};

		std::vector<Content> files;
		for (const auto& [type, typeFiles] : content->contentTypeToContentMap) {
			files.insert(files.end(), typeFiles.begin(), typeFiles.end());
		}

		return files;

	}

	const Ref<ContentDirectory> ContentDiscovery::GetDirectory(const std::string& path) {

		if (!content || !content->contentDirectories.contains(path))
			return {};

		return content->contentDirectories.at(path);

	}

	void ContentDiscovery::Update() {

		if (!contentDiscoveryJob.HasFinished())
			return;

		if (hasNextContent) {
			// Swap here to not immediately release the memory of content (causes stutter due to freeing memory)
			std::swap(content, nextContent);
			hasNextContent = false;
			finishedCount = startedCount;
		}

		bool canRediscover = (Clock::Get() - lastDiscoveryTime) >= discoverFrequency || !content
			|| content->contentDirectories.empty() || execute;

		if (canRediscover) {
			lastDiscoveryTime = Clock::Get();
			// Changes from here on need another discovery
			execute = false;
			startedCount++;
			hasNextContent = true;
			JobSystem::Execute(contentDiscoveryJob, [&](JobData&) {
				// Now release the swapped memory here in the job async
				nextContent.reset();

				nextContent = PerformContentDiscovery();
			});

			return;
		}
	}

	Ref<ContentDiscovery::DiscoveredContent> ContentDiscovery::PerformContentDiscovery() {

		auto dataDirectory = Loader::AssetLoader::GetDataDirectory();

		auto rootDirectory = CreateRef<ContentDirectory>({
			.path = dataDirectory
		});
		auto result = CreateRef<DiscoveredContent>({
			.rootDirectory = rootDirectory,
		});
		result->contentDirectories[dataDirectory] = rootDirectory;

		DiscoverDirectory(rootDirectory, result);

		return result;

	}

	void ContentDiscovery::DiscoverDirectory(const Ref<ContentDirectory>& directory, const Ref<DiscoveredContent>& result) {

		auto dataDirectory = Loader::AssetLoader::GetDataDirectory();

		// Directories can be renamed or deleted in the meantime
		std::error_code directoryError;
		auto iterator = std::filesystem::directory_iterator(directory->path,
			std::filesystem::directory_options::skip_permission_denied, directoryError);
		for (; !directoryError && iterator != std::filesystem::directory_iterator(); iterator.increment(directoryError)) {
			const auto& dirEntry = *iterator;
			std::error_code errorCode;
			auto path = Common::Path::Normalize(dirEntry.path().string());
			auto assetPath = Common::Path::GetRelative(dataDirectory, path);
			if (assetPath.starts_with('/'))
				assetPath.erase(assetPath.begin());
			assetPath = Common::Path::Normalize(assetPath);

			if (dirEntry.is_directory(errorCode)) {
				auto dirname = Common::Path::GetFileName(dirEntry.path().string());
				std::transform(dirname.begin(), dirname.end(), dirname.begin(), ::tolower);

				auto childDirectory = CreateRef<ContentDirectory>({
						.name = dirname,
						.path = dirEntry.path(),
						.assetPath = assetPath
					});
				directory->directories.push_back(childDirectory);

				auto childPath = childDirectory->path.string();
				std::replace(childPath.begin(), childPath.end(), '\\', '/');
				result->contentDirectories[childPath] = childDirectory;
				DiscoverDirectory(childDirectory, result);
				continue;
			}

			auto filename = Common::Path::GetFileName(path);
			std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

			std::string fileType = Common::Path::GetFileType(filename);
			if (!Content::contentTypeMapping.contains(fileType))
				continue;

			auto contentType = Content::contentTypeMapping.at(fileType);
			auto fileSize = dirEntry.file_size(errorCode);
			if (errorCode)
				fileSize = 0;
			auto writeTime = dirEntry.last_write_time(errorCode);
			auto lastWriteTime = errorCode ? int64_t(0) : int64_t(writeTime.time_since_epoch().count());
			directory->files.emplace_back(Content {
				.name = filename,
				.path = path,
				.assetPath = assetPath,
				.type = contentType,
				.size = fileSize,
				.lastWriteTime = lastWriteTime,
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