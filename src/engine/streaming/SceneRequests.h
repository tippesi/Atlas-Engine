#pragma once

#include "../System.h"
#include "MeshStreaming.h"
#include "TextureStreaming.h"
#include "ImpostorStreaming.h"
#include "../scene/GPUScene.h"

#include <unordered_map>
#include <vector>

namespace Atlas::Scene {

    class Scene;

}

namespace Atlas::Streaming {

    /**
     * Requests the meshes, impostors and texture resolutions of a scene from the streaming. The GPU culling measures the
     * distances of the closest instances of each mesh, skinned meshes, clutter and the terrain are measured from the
     * main camera.
     */
    class SceneRequests {

    public:
        void Request(Scene::Scene* scene);

    private:
        Scene::GPUScene::MeshDistances meshDistances;
        std::unordered_map<Material*, float> materialResolutions;
        std::vector<TextureStreaming::MaterialRequest> materialRequests;
        std::vector<MeshStreaming::MeshRequest> meshRequests;
        std::vector<ImpostorStreaming::ImpostorRequest> impostorRequests;
        size_t requestOffset = 0;

    };

}
