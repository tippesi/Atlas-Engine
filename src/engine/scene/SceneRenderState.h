#pragma once

#include "Entity.h"
#include "GPUScene.h"
#include "jobsystem/JobGroup.h"
#include "texture/Texture2D.h"
#include "texture/Texture2DArray.h"
#include "texture/Cubemap.h"

#include "buffer/UniformBuffer.h"

#include "renderer/helper/CommonStructures.h"
#include "renderer/helper/RenderList.h"

#include "Wind.h"
#include "Clutter.h"
#include "components/CameraComponent.h"
#include "components/TextComponent.h"
#include "lighting/Sky.h"
#include "lighting/Fog.h"
#include "lighting/IrradianceVolume.h"
#include "lighting/RTGI.h"
#include "lighting/SSGI.h"
#include "lighting/Reflection.h"
#include "lighting/SSS.h"
#include "postprocessing/PostProcessing.h"
#include "ocean/Ocean.h"
#include "terrain/Terrain.h"
#include "raytracing/RayTracingWorld.h"
#include "streaming/StreamingManager.h"

#include <functional>
#include <unordered_map>
#include <optional>

namespace Atlas::Scene {

    class Scene;

    struct LightEntity {
        Entity entity;
        LightComponent comp;
        // The shadow is rendered in this frame, the request of the shadow was taken by the extraction
        bool updateShadow = false;
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

    struct MaterialGraphData {
        // Entry in the material buffer, graphs with a surface have their own one for the G-buffer
        uint32_t materialIdx = 0;
        // Position of the values in the parameter buffer
        uint32_t parameterOffset = 0;
    };

    struct MaterialGraphCache {
        struct TextureParameter {
            const ResourceHandle<Texture::Texture2D>* handle = nullptr;
            uint32_t offset = 0;
            MaterialGraph::DefaultTexture fallback = MaterialGraph::DefaultTexture::White;
        };

        // A new material at the same address must not find this entry
        Ref<Material> material;
        Ref<const MaterialGraph::CompiledGraph> compiled;
        uint32_t version = 0;
        std::vector<uint32_t> words;
        std::vector<TextureParameter> textures;
        uint32_t lastUsedFrame = 0;
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

        // Bindless devices keep the instances on the GPU and cull them there
        void UpdateGPUScene(const std::vector<ECS::Entity>& dirtyEntities, const std::vector<ECS::Entity>& lastDirtyEntities);

        void FillMainRenderPass();

        void FillShadowRenderPass(Entity entity);

        void CullAndSortLights();

        // Requests the meshes and the texture resolutions the closest instances of the meshes need
        void RequestStreamedResources();

        void WaitForAsyncWorkCompletion();

        // Waits for the jobs without clearing the render list, which can still be in use by a render thread
        void WaitForJobs();

        // Copies what the renderers read from the scene, the scene can be updated for the next frame while this one renders
        void Extract();

        const LightEntity* GetLight(ECS::Entity entity) const;

        bool HasMainLight() const { return mainLight && mainLight->isMain; }

        Scene* scene;

        // Extracted scene state. The settings are copies, objects with GPU state the renderers update are shared
        CameraComponent camera;
        bool hasCamera = false;
        // A directional light, the main one if there is one
        std::optional<LightComponent> mainLight;
        std::vector<LightEntity> allLights;
        std::vector<TextComponent> texts;
        float time = 0.0f;
        float deltaTime = 0.0f;

        Wind wind;
        // The clouds are a copy as well, the atmosphere, the probe and the irradiance volume are shared
        Lighting::Sky sky;
        // Requests of the shared atmosphere, taken by the extraction and cleared when they are rendered
        bool atmosphereNeedsUpdate = false;
        Ref<Lighting::Fog> fog;
        Ref<Lighting::RTGI> rtgi;
        Ref<Lighting::SSGI> ssgi;
        Ref<Lighting::Reflection> reflection;
        Ref<Lighting::SSS> sss;
        PostProcessing::PostProcessing postProcessing;

        // Only set while the volume is enabled
        Ref<Lighting::IrradianceVolume> irradianceVolume;
        Ref<Ocean::Ocean> ocean;
        ResourceHandle<Terrain::Terrain> terrain;
        Ref<Clutter> clutter;
        Ref<RayTracing::RayTracingWorld> rayTracingWorld;

        RenderList renderList;
        GPUScene gpuScene;

        Buffer::Buffer materialBuffer;
        Buffer::Buffer materialGraphBuffer;
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
        // Only filled for materials with a graph
        std::vector<Ref<Material>> graphMaterials;
        std::unordered_map<void*, MaterialGraphData> materialGraphMap;
        std::unordered_map<Material*, MaterialGraphCache> materialGraphCaches;
        std::vector<uint32_t> materialGraphParameters;
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
        JobGroup fillGPUShadowRenderPassesJob{ "GPU shadow render pass update", JobPriority::High};
        JobGroup gpuSceneUpdateJob{ "GPU scene update", JobPriority::High };
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

        void UpdateMaterialGraphParameters(const std::function<uint32_t(const ResourceHandle<Texture::Texture2D>&,
            MaterialGraph::DefaultTexture)>& requestTexture);

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

        GPUScene::MeshDistances meshDistances;
        std::unordered_map<Material*, float> materialResolutions;
        std::vector<Streaming::TextureStreaming::MaterialRequest> materialRequests;
        std::vector<Streaming::MeshStreaming::MeshRequest> meshRequests;
        std::vector<Streaming::ImpostorStreaming::ImpostorRequest> impostorRequests;
        size_t streamingRequestOffset = 0;

    };

}
