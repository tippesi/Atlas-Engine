#ifndef AE_RENDERLIST_H
#define AE_RENDERLIST_H

#include "System.h"
#include "actor/ActorBatch.h"
#include "actor/MeshActor.h"
#include "actor/DecalActor.h"
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

            std::map<Mesh::Mesh*, std::vector<Actor::MeshActor*>> meshToActorMap;
            std::map<Mesh::Mesh*, MeshInstances> meshToInstancesMap;
        };

		RenderList();

        void NewFrame();

        void NewMainPass();

        void NewShadowPass(Lighting::Light* light, uint32_t layer);

        Pass& GetMainPass();

        Pass& GetShadowPass(Lighting::Light* light, uint32_t layer);

		void Add(Actor::MeshActor *actor);

		void Update(Camera* camera);

		void FillBuffers();

        std::vector<mat4> currentActorMatrices;
        std::vector<mat4> lastActorMatrices;
        std::vector<mat4> impostorMatrices;

        Ref<Graphics::MultiBuffer> currentMatricesBuffer;
        Ref<Graphics::MultiBuffer> lastMatricesBuffer;
        Ref<Graphics::MultiBuffer> impostorMatricesBuffer;

        std::vector<Pass> passes;

	};

}

#endif