#pragma once

#include "scene/Scene.h"

namespace Atlas::Editor {

	class Config {

	public:
		bool darkMode = true;
		bool pathTrace = false;

        std::vector<ResourceHandle<Scene::Scene>> openedScenes;

	};

}