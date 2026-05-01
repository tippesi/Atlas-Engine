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
    };

    class SceneRenderState {

    public:
        SceneRenderState(Scene* scene);
        
        ~SceneRenderState();

        void NewFrame();

        void PrepareMaterials();

        void UpdateBlasBindlessData();

        void UpdateTextureBindlessData();

        void UpdateOtherTextureBindlessData();

        void FillMainRenderPass();

        void FillShadowRenderPass(Entity entity);

        void CullAndSortLights();

        void WaitForAsyncWorkCompletion();

        Scene* scene;
        RenderList renderList;

        Buffer::Buffer materialBuffer;
        Buffer::Buffer lightBuffer;
        Buffer::Buffer volumetricLightBuffer;
        Buffer::Buffer volumetricShadowBuffer;

        std::vector<Ref<Graphics::Image>> textures;
        std::vector<Ref<Graphics::Image>> textureArrays;
        std::vector<Ref<Graphics::Image>> cubemaps;
        std::vector<Ref<Graphics::Buffer>> blasBuffers;
        std::vector<Ref<Graphics::Buffer>> triangleBuffers;
        std::vector<Ref<Graphics::Buffer>> bvhTriangleBuffers;
        std::vector<Ref<Graphics::Buffer>> triangleOffsetBuffers;

        std::vector<ResourceHandle<Mesh::Mesh>> meshes;
        std::vector<Terrain::TerrainNode*> terrainLeafNodes;
        std::vector<Renderer::PackedMaterial> materials;
        std::unordered_map<void*, uint16_t> materialMap;
        std::unordered_map<Ref<Texture::Texture2D>, uint32_t> textureToBindlessIdx;
        std::unordered_map<Ref<Texture::Texture2DArray>, uint32_t> textureArrayToBindlessIdx;
        std::unordered_map<Ref<Texture::Cubemap>, uint32_t> cubemapToBindlessIdx;
        std::unordered_map<Ref<RayTracing::BLAS>, uint32_t> blasToBindlessIdx;

        std::vector<LightEntity> lightEntities;
        std::vector<Renderer::Light> lights;
        std::vector<Renderer::VolumetricLight> volumetricLights;
        std::vector<Renderer::Shadow> volumetricShadows;

        JobGroup materialUpdateJob{ "Material update", JobPriority::High};
        JobGroup rayTracingWorldUpdateJob{ "Ray tracing world update", JobPriority::High};
        JobGroup bindlessBlasMapUpdateJob{ "Blas map update", JobPriority::High};
        JobGroup bindlessTextureMapUpdateJob{ "Texture map update", JobPriority::High };
        JobGroup bindlessOtherTextureMapUpdateJob{ "Other texture map update", JobPriority::High };
        JobGroup prepareBindlessBlasesJob{ "Prepare blases", JobPriority::High };
        JobGroup newFrameRenderListJob { "New frame render list", JobPriority::High};
        JobGroup fillMainRenderPassJob{ "Main render pass update", JobPriority::High};
        JobGroup fillShadowRenderPassesJob{ "Shadow render pass update", JobPriority::High};
        JobGroup cullAndSortLightsJob{ "Cull and sort lights", JobPriority::High};

    };

}
