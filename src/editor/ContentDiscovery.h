#pragma once

#include "Content.h"
#include "System.h"
#include "jobsystem/JobSystem.h"

#include <vector>
#include <shared_mutex>

namespace Atlas::Editor {

	struct ContentDirectory {
		std::filesystem::path path;
		std::string assetPath;

		std::vector<Ref<ContentDirectory>> directories;
		std::vector<Content> files;
	};

	class ContentDiscovery {

	public:
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

		static const float discoverFrequency;
		static float lastDiscoveryTime;

	};

}