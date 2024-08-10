#pragma once

#include "scene/Scene.h"

namespace Atlas::Editor {

	struct ContentBrowserSettings {
        bool searchRecursively = true;
        bool filterRecursively = false;
    };

	class Config {

	public:
		bool darkMode = true;
		bool pathTrace = false;
		bool vsync = true;

		ContentBrowserSettings contentBrowserSettings;

        std::vector<ResourceHandle<Scene::Scene>> openedScenes;

	};

}