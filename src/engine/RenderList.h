#pragma once

#include "System.h"
#include "scene/Entity.h"
#include "lighting/Light.h"

#include "graphics/Buffer.h"

#include <map>
#include <vector>

namespace Atlas {

    class RenderList {

    public:
        struct MeshInstances {
            size_t offset;
            size_t count;

            size_t impostorOffset;
            size_t impostorCount;
        };

        enum class RenderPassType {
            Main = 0,
            Shadow = 1
        };

        struct Pass {
            RenderPassType type;

            Lighting::Light* light;
            uint32_t layer;

            std::map<size_t, std::vector<Scene::Entity>> meshToActorMap;
            std::map<size_t, MeshInstances> meshToInstancesMap;
            std::map<size_t, ResourceHandle<Mesh::Mesh>> meshIdToMeshMap;
        };

        RenderList();

        void NewFrame();

        void NewMainPass();

        void NewShadowPass(Lighting::Light* light, uint32_t layer);

        Pass* GetMainPass();

        Pass* GetShadowPass(const Lighting::Light* light, const uint32_t layer);

        void Add(Scene::Entity entity, Scene::Components::MeshComponent& meshComponent);

        void Update(Camera* camera);

        void FillBuffers();

        std::vector<mat3x4> currentActorMatrices;
        std::vector<mat3x4> lastActorMatrices;
        std::vector<mat3x4> impostorMatrices;

        Ref<Graphics::MultiBuffer> currentMatricesBuffer;
        Ref<Graphics::MultiBuffer> lastMatricesBuffer;
        Ref<Graphics::MultiBuffer> impostorMatricesBuffer;

        std::vector<Pass> passes;

    };

}