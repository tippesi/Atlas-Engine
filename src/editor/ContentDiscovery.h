#pragma once

#include "Content.h"
#include "System.h"
#include "jobsystem/JobSystem.h"

#include <vector>
#include <shared_mutex>
#include <atomic>

namespace Atlas::Editor {

	struct ContentDirectory {
		std::string name;
		std::filesystem::path path;
		std::string assetPath;

		std::vector<Ref<ContentDirectory>> directories;
		std::vector<Content> files;
	};

	class ContentDiscovery {

	public:
		// The content is discovered again afterwards, e.g. in the directory of another project
		static void Shutdown() { JobSystem::Wait(contentDiscoveryJob); content.reset(); nextContent.reset(); hasNextContent = false; }

		// Returns the number of the discovery which finds everything that changed until now
		static size_t Execute();

		static bool HasFinished(size_t discovery) { return finishedCount >= discovery; }

		static const Ref<ContentDirectory> GetContent();

		static std::vector<Content> GetContent(const ContentType type);

		static std::vector<Content> GetAllContent();

		static const Ref<ContentDirectory> GetDirectory(const std::string& path);

		static void Update();

	private:
		struct DiscoveredContent {
			Ref<ContentDirectory> rootDirectory;
			std::map<ContentType, std::vector<Content>> contentTypeToContentMap;
			std::map<std::string, Ref<ContentDirectory>> contentDirectories;
		};

		static Ref<DiscoveredContent> PerformContentDiscovery();

		static void DiscoverDirectory(const Ref<ContentDirectory>& directory, const Ref<DiscoveredContent>& result);

		static Ref<DiscoveredContent> content;
		static Ref<DiscoveredContent> nextContent;
		static JobGroup contentDiscoveryJob;
		// The last job found content which isn't shown yet
		static bool hasNextContent;

		static std::atomic_bool execute;
		static std::atomic_size_t startedCount;
		static size_t finishedCount;
		static const float discoverFrequency;
		static float lastDiscoveryTime;

	};

}