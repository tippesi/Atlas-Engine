#pragma once

#include "../System.h"
#include "../RenderList.h"
#include "Renderer.h"
#include "ImpostorShadowRenderer.h"

#include <mutex>
#include <map>

namespace Atlas {

    namespace Renderer {

        class ShadowRenderer : public Renderer {

        public:
            ShadowRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList, RenderList* renderList);

        private:
            Ref<Graphics::FrameBuffer> GetOrCreateFrameBuffer(Lighting::Light* light);

            PipelineConfig GetPipelineConfigForSubData(Mesh::MeshSubData* subData,
                ResourceHandle<Mesh::Mesh>& mesh, Ref<Graphics::FrameBuffer>& frameBuffer);

            using LightMap = std::map<Lighting::Light*, Ref<Graphics::FrameBuffer>>;

            struct PushConstants {
                mat4 lightSpaceMatrix;
                uint32_t vegetation;
                uint32_t invertUVs;
                float windTextureLod;
                float windBendScale;
                float windWiggleScale;
            };

            LightMap lightMap;

            ImpostorShadowRenderer impostorRenderer;

        };

    }

}