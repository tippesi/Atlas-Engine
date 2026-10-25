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

    struct MaterialTextureIndices {
        uint32_t baseColor = 0;
        uint32_t opacity = 0;
        uint32_t normal = 0;
        uint32_t roughness = 0;
        uint32_t metalness = 0;
        uint32_t ao = 0;
        uint32_t height = 0;
        uint32_t emissive = 0;
    };

    class SceneRenderState {

    public:
        SceneRenderState(Scene* scene);
        
        ~SceneRenderState();

        void NewFrame();

        void PrepareMaterials();

        void UpdateBlasBindlessData(bool rayTracing);

        uint32_t GetBindlessTextureIdx(const Ref<Texture::Texture2D>& texture) const;

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
        std::vector<Ref<Material>> uniqueMaterials;
        std::vector<MaterialTextureIndices> materialTextureIndices;
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
        JobGroup bindlessOtherTextureMapUpdateJob{ "Other texture map update", JobPriority::High };
        JobGroup prepareBindlessBlasesJob{ "Prepare blases", JobPriority::High };
        JobGroup newFrameRenderListJob { "New frame render list", JobPriority::High};
        JobGroup fillMainRenderPassJob{ "Main render pass update", JobPriority::High};
        JobGroup fillShadowRenderPassesJob{ "Shadow render pass update", JobPriority::High};
        JobGroup cullAndSortLightsJob{ "Cull and sort lights", JobPriority::High};

    private:
        struct BindlessTextureSlot {
            Ref<Texture::Texture2D> texture;
            uint32_t lastUsedFrame = 0;
        };

        enum class DefaultTextureSlot : uint32_t {
            White = 0,
            Black,
            Normal,
            Count
        };

        void UpdateTextureBindlessData();

        uint32_t RequestBindlessTexture(const Ref<Texture::Texture2D>& texture, DefaultTextureSlot defaultSlot);

        void CreateDefaultBindlessTextures();

        void EvictUnusedBindlessTextures();

        const uint32_t maxNewTextureSlotsPerFrame = 256;
        const uint32_t textureEvictionChecksPerFrame = 256;
        const uint32_t textureEvictionFrameCount = 60;

        std::vector<BindlessTextureSlot> textureSlots;
        std::vector<uint32_t> freeTextureSlots;

        uint32_t textureFrameCount = 0;
        uint32_t newTextureSlotCount = 0;
        uint32_t textureEvictionIdx = 0;

    };

}
