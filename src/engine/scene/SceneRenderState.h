#pragma once

#include "jobsystem/JobGroup.h"
#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "texture/Cubemap.h"

#include "buffer/UniformBuffer.h"

#include "renderer/helper/CommonStructures.h"

#include <unordered_map>

namespace Atlas::Scene {

    class Scene;

    class SceneRenderState {

    public:
        SceneRenderState(Scene* scene);

        void PrepareMaterials();

        void UpdateMeshBindlessData();

        void UpdateTextureBindlessData();

        void UpdateOtherTextureBindlessData();

        Scene* scene;

        Buffer::Buffer materialBuffer;
        std::vector<Ref<Graphics::Image>> images;
        std::vector<Ref<Graphics::Buffer>> blasBuffers;
        std::vector<Ref<Graphics::Buffer>> triangleBuffers;
        std::vector<Ref<Graphics::Buffer>> bvhTriangleBuffers;
        std::vector<Ref<Graphics::Buffer>> triangleOffsetBuffers;

        std::vector<Renderer::PackedMaterial> materials;
        std::unordered_map<void*, uint16_t> materialMap;
        std::unordered_map<Ref<Texture::Texture2D>, uint32_t> textureToBindlessIdx;
        std::unordered_map<Ref<Texture::Texture2DArray>, uint32_t> textureArrayToBindlessIdx;
        std::unordered_map<Ref<Texture::Cubemap>, uint32_t> cubemapToBindlessIdx;
        std::unordered_map<size_t, uint32_t> meshIdToBindlessIdx;

        JobGroup materialUpdateJob { JobPriority::High };
        JobGroup rayTracingWorldUpdateJob { JobPriority::High };
        JobGroup bindlessMeshMapUpdateJob { JobPriority::High };
        JobGroup bindlessTextureMapUpdateJob { JobPriority::High };
        JobGroup bindlessOtherTextureMapUpdateJob { JobPriority::High };
        JobGroup prepareBindlessMeshesJob { JobPriority::High };
        JobGroup prepareBindlessTexturesJob { JobPriority::High };

    };

}