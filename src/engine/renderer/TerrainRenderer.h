#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class TerrainRenderer : public Renderer {

        public:
            TerrainRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList,
                std::unordered_map<void*, uint16_t> materialMap);

        private:
            void GetUniforms();

            struct TerrainMaterial {
                uint32_t idx;
                
                float roughness;
                float metalness;
                float ao;
                float displacementScale;
                float normalScale;
                float tiling;

                float padding1;
            };

            struct alignas(16) Uniforms {

                vec4 frustumPlanes[6];

                float heightScale;
                float displacementDistance;

                float tessellationFactor;
                float tessellationSlope;
                float tessellationShift;
                float maxTessellationLevel;

            };

            struct alignas(16) PushConstants {

                float nodeSideLength;
                float tileScale;
                float patchSize;
                float normalTexelSize;

                float leftLoD;
                float topLoD;
                float rightLoD;
                float bottomLoD;

                vec2 nodeLocation;

            };

            PipelineConfig detailConfig;
            PipelineConfig distanceConfig;

            Buffer::UniformBuffer uniformBuffer;
            Buffer::Buffer terrainMaterialBuffer;

            PipelineConfig GeneratePipelineConfig(RenderTarget* target,
                Ref<Terrain::Terrain>& terrain, bool detailConfig,
                bool materialMappingConfig);
        };

    }

}