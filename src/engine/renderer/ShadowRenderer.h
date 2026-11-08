#pragma once

#include "../System.h"
#include "helper/RenderList.h"
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

            // Switches to the pipelines of the graph once they are created
            void UpdateGraphPipelines(Mesh::MeshSubData* subData, Mesh::Mesh* mesh,
                const Graphics::AttachmentFormats& attachmentFormats);

            struct alignas(16) PushConstants {
                mat4 lightSpaceMatrix;
                uint32_t vegetation;
                uint32_t invertUVs;
                float windTextureLod;
                float windBendScale;
                float windWiggleScale;
                uint32_t textureID;
                uint32_t jointOffset;
                uint32_t jointCount;
                uint32_t instanceOffset;
                // Only part of the push constants of graphs, the others don't push it
                uint32_t materialGraphOffset;
            };

            std::set<Scene::Entity> processedLights;

            ImpostorShadowRenderer impostorRenderer;

            // Draws culled on the GPU have a draw command
            std::vector<std::tuple<Mesh::MeshSubData*, Hash, Mesh::Mesh*, const RenderList::GPUDraw*>> subDatas;


        };

    }

}
