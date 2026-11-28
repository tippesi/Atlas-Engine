#pragma once

#include "scene/Scene.h"

#include <imgui.h>

#include <map>

namespace Atlas::Editor {

	class Config {

	public:
		bool darkMode = true;
		bool pathTrace = false;
		bool vsync = true;
		// Zero follows the scale of the display set in the system settings
		float uiScale = 0.0f;

		std::vector<ResourceHandle<Scene::Scene>> openedScenes;

		// Names of the drawers in the status bar, in their order
		std::vector<std::string> statusBarDrawers = { "Content browser", "Log" };

		// Changed shortcuts by the name of their command ("File: Save scene"), zero without one
		std::map<std::string, ImGuiKeyChord> shortcuts;

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