#pragma once

#include "Entity.h"
#include "jobsystem/JobGroup.h"
#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "texture/Cubemap.h"

#include "buffer/UniformBuffer.h"

#include "renderer/helper/CommonStructures.h"
#include "renderer/helper/RenderList.h"

#include <unordered_map>

namespace Atlas::Scene {

    class Scene;

    struct LightEntity {
        Entity entity;
        LightComponent comp;
        int32_t mapIdx = -1;
    };

    class SceneRenderState {

    public:
        SceneRenderState(Scene* scene);
        
        ~SceneRenderState();

        void PrepareMaterials();

        void UpdateMeshBindlessData();

        void UpdateTextureBindlessData();

        void UpdateOtherTextureBindlessData();

        void FillRenderList();

        void CullAndSortLights();

        void WaitForAsyncWorkCompletion();

        Scene* scene;
        RenderList renderList;

        Buffer::Buffer materialBuffer;
        Buffer::Buffer lightBuffer;
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
        std::vector<LightEntity> lightEntities;

        JobGroup materialUpdateJob{ JobPriority::High };
        JobGroup rayTracingWorldUpdateJob{ JobPriority::High };
        JobGroup bindlessMeshMapUpdateJob{ JobPriority::High };
        JobGroup bindlessTextureMapUpdateJob{ JobPriority::High };
        JobGroup bindlessOtherTextureMapUpdateJob{ JobPriority::High };
        JobGroup prepareBindlessMeshesJob{ JobPriority::High };
        JobGroup prepareBindlessTexturesJob{ JobPriority::High };
        JobGroup fillRenderListJob{ JobPriority::High };
        JobGroup cullAndSortLightsJob{ JobPriority::High };

    };

}
