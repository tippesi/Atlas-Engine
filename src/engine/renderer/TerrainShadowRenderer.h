#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class TerrainShadowRenderer : public Renderer {

        public:
            TerrainShadowRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList);

        private:
            using LightMap = std::map<Lighting::Light*, Ref<Graphics::FrameBuffer>>;

            struct alignas(16) PushConstants {

                mat4 lightSpaceMatrix;

                float nodeSideLength;
                float tileScale;
                float patchSize;
                float heightScale;

                float leftLoD;
                float topLoD;
                float rightLoD;
                float bottomLoD;

                vec2 nodeLocation;

            };

            PipelineConfig GeneratePipelineConfig(Ref<Graphics::FrameBuffer>& framebuffer,
                Ref<Terrain::Terrain>& terrain);

            Ref<Graphics::FrameBuffer> GetOrCreateFrameBuffer(Lighting::Light* light);

            LightMap lightMap;

            Buffer::UniformBuffer uniformBuffer;

        };

    }

}