#pragma once

#include "scene/Scene.h"

namespace Atlas::Editor {

    class DataCreator {

    public:
        static Ref<Scene::Scene> CreateScene(const std::string& name, vec3 min, vec3 max, int32_t depth);

    };

}