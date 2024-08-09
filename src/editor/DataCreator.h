#pragma once

#include "scene/Scene.h"

namespace Atlas::Editor {

    class DataCreator {

    public:
        static Ref<Scene::Scene> CreateScene(const std::string& name, vec3 min, vec3 max, int32_t depth);

        static Ref<Scene::Scene> CreateSceneFromMesh(const std::string& filename,vec3 min, vec3 max, int32_t depth,
            bool invertUVs, bool addRigidBodies, bool makeMeshesStatic);

    };

}