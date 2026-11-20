#pragma once

#include "../System.h"
#include "helper/RenderList.h"
#include "helper/SubDataDrawer.h"
#include "Renderer.h"
#include "ImpostorShadowRenderer.h"

#include <mutex>
#include <set>
#include <tuple>

namespace Atlas {

    namespace Renderer {

        class ShadowRenderer : public Renderer {

        public:
            ShadowRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, RenderList* renderList);

        private:
            void ProcessPass(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, 
                RenderList* renderList, Ref<RenderList::Pass> shadowPass);

            Graphics::RenderingDesc GetRendering(const LightComponent& light, uint32_t layer);

            PipelineConfig GetPipelineConfigForSubData(Mesh::MeshSubData* subData, Mesh::Mesh* mesh,
                const Graphics::AttachmentFormats& attachmentFormats, const MaterialGraph::CompiledGraph* graph = nullptr);

            struct alignas(16) PushConstants {
                mat4 lightSpaceMatrix;
                uint32_t vegetation;
                uint32_t invertUVs;
                float windTextureLod;
                float windBendScale;
                float windWiggleScale;
                uint32_t textureID;
                uint32_t deformOffset;
                uint32_t jointCount;
                uint32_t instanceOffset;
                float uvAnimationX;
                float uvAnimationY;
                float uvTiling;
                // Only part of the push constants of graphs, the others don't push it
                uint32_t materialGraphOffset;
            };

            std::set<Scene::Entity> processedLights;

            ImpostorShadowRenderer impostorRenderer;

            Helper::SubDataDrawer drawer;


        };

    }

}
