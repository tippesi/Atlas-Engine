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
            struct alignas(16) PushConstants {

                mat4 lightSpaceMatrix;
                vec4 translation;

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

            PipelineConfig GeneratePipelineConfig(const Graphics::AttachmentFormats& attachmentFormats,
                Ref<Terrain::Terrain>& terrain);

            Buffer::UniformBuffer uniformBuffer;

        };

    }

}