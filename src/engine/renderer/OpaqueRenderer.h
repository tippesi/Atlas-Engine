#pragma once

#include "../System.h"
#include "helper/RenderList.h"
#include "helper/SubDataDrawer.h"

#include "Renderer.h"

#include <mutex>
#include <tuple>

namespace Atlas {

    namespace Renderer {

        class OpaqueRenderer : public Renderer {

        public:
            OpaqueRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, 
                RenderList* renderList, std::unordered_map<void*, uint16_t>& materialMap, bool lateDraws = false);

        private:
            PipelineConfig GetPipelineConfigForSubData(Mesh::MeshSubData* subData,
                Mesh::Mesh* mesh, const Ref<RenderTarget>& target, const MaterialGraph::CompiledGraph* graph = nullptr);

            Helper::SubDataDrawer drawer;

            struct alignas(16) PushConstants {
                uint32_t vegetation;
                uint32_t invertUVs;
                uint32_t twoSided;
                uint32_t staticMesh;
                uint32_t materialIdx;
                float normalScale;
                float displacementScale;
                float windTextureLod;
                float windBendScale;
                float windWiggleScale;
                float uvAnimationX;
                float uvAnimationY;
                float uvTiling;
                uint32_t baseColorTextureIdx;
                uint32_t opacityTextureIdx;
                uint32_t normalTextureIdx;
                uint32_t roughnessTextureIdx;
                uint32_t metalnessTextureIdx;
                uint32_t aoTextureIdx;
                uint32_t heightTextureIdx;
                uint32_t emissiveTextureIdx;
                uint32_t deformOffset;
                uint32_t jointCount;
                uint32_t instanceOffset;
                // Level of detail of host culled draws, the GPU culled ones add their draw index
                uint32_t lod;
                // Only part of the push constants of graphs, the others don't push it
                uint32_t materialGraphOffset;
            };


        };


    }

}
