#ifndef AE_RTDATA_H
#define AE_RTDATA_H

#include "../System.h"

#include "RTStructures.h"
#include "../actor/MeshActor.h"
#include "../texture/TextureAtlas.h"

#include <vector>
#include <unordered_map>

namespace Atlas {

    // Forward-declare class to use it as a friend
    namespace Renderer::Helper {
        class RayTracingHelper;
    }

    namespace Scene {

        class Scene;

        class RTData {

            friend Renderer::Helper::RayTracingHelper;

        public:
            RTData() = default;

            RTData(Scene* scene);

            void Build();

            void Update();

            void UpdateMaterials(bool updateTextures = false);

            void UpdateTextures();

            bool IsValid();

            void Clear();

        private:
            void UpdateMaterials(std::vector<GPUMaterial>& materials, bool updateTextures);

            GPUTexture CreateGPUTextureStruct(std::vector<Texture::TextureAtlas::Slice> slices);

            GPUTextureLevel CreateGPUTextureLevelStruct(Texture::TextureAtlas::Slice slice);

            void BuildForSoftwareRayTracing();

            void BuildForHardwareRayTracing();

            std::vector<GPUBVHInstance> UpdateForSoftwareRayTracing(std::vector<GPUBVHInstance>& gpuBvhInstances,
                std::vector<Volume::AABB>& actorAABBs);

            void UpdateForHardwareRayTracing(std::vector<Actor::MeshActor*>& actors);

            Scene* scene;

            Ref<Graphics::TLAS> tlas;
            Ref<Graphics::Buffer> instanceBuffer;
            std::vector<Ref<Graphics::BLAS>> blases;

            Buffer::Buffer triangleBuffer;
            Buffer::Buffer bvhTriangleBuffer;
            Buffer::Buffer materialBuffer;
            Buffer::Buffer bvhInstanceBuffer;
            Buffer::Buffer tlasNodeBuffer;
            Buffer::Buffer blasNodeBuffer;

            Texture::TextureAtlas baseColorTextureAtlas;
            Texture::TextureAtlas opacityTextureAtlas;
            Texture::TextureAtlas normalTextureAtlas;
            Texture::TextureAtlas roughnessTextureAtlas;
            Texture::TextureAtlas metalnessTextureAtlas;
            Texture::TextureAtlas aoTextureAtlas;

            std::vector<GPULight> triangleLights;

            std::unordered_map<Material*, int32_t> materialAccess;
            std::unordered_map<size_t, GPUMesh> meshInfo;

            bool hardwareRayTracing = false;

            std::atomic_bool isValid = false;
            std::mutex mutex;

        };

    }

}

#endif