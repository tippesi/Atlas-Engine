#pragma once

#include "scene/Scene.h"

namespace Atlas::Editor {

	class Config {

	public:
		bool darkMode = true;
		bool pathTrace = false;
		bool vsync = true;

		std::vector<ResourceHandle<Scene::Scene>> openedScenes;

		// Most recently opened scene paths, newest first
		std::vector<std::string> recentScenes;

		void AddRecentScene(const std::string& path) {
			std::erase(recentScenes, path);
			recentScenes.insert(recentScenes.begin(), path);
			if (recentScenes.size() > 10)
				recentScenes.resize(10);
		}

	};

}