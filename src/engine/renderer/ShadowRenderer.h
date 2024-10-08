#pragma once

#include "../System.h"
#include "helper/RenderList.h"
#include "Renderer.h"
#include "ImpostorShadowRenderer.h"

#include <mutex>
#include <map>
#include <tuple>

namespace Atlas {

    namespace Renderer {

        class ShadowRenderer : public Renderer {

        public:
            ShadowRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, RenderList* renderList);

        private:
            void ProcessPass(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, Ref<RenderList::Pass> shadowPass);

            Ref<Graphics::FrameBuffer> GetOrCreateFrameBuffer(Scene::Entity entity);

            PipelineConfig GetPipelineConfigForSubData(Mesh::MeshSubData* subData,
                Mesh::Mesh* mesh, Ref<Graphics::FrameBuffer>& frameBuffer);

            using LightMap = std::map<Scene::Entity, Ref<Graphics::FrameBuffer>>;

            struct alignas(16) PushConstants {
                mat4 lightSpaceMatrix;
                uint32_t vegetation;
                uint32_t invertUVs;
                float windTextureLod;
                float windBendScale;
                float windWiggleScale;
                uint32_t textureID;
            };

            LightMap lightMap;

            ImpostorShadowRenderer impostorRenderer;

            std::vector<std::tuple<Mesh::MeshSubData*, Hash, Mesh::Mesh*>> subDatas;

        };

    }

}
