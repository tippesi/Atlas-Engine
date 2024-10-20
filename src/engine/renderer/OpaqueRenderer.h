#pragma once

#include "../System.h"
#include "helper/RenderList.h"

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
                RenderList* renderList, std::unordered_map<void*, uint16_t> materialMap);

        private:
            PipelineConfig GetPipelineConfigForSubData(Mesh::MeshSubData* subData,
                Mesh::Mesh* mesh, const Ref<RenderTarget>& target);

            std::vector<std::tuple<Mesh::MeshSubData*, Hash, Mesh::Mesh*>> subDatas;

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
                uint32_t baseColorTextureIdx;
                uint32_t opacityTextureIdx;
                uint32_t normalTextureIdx;
                uint32_t roughnessTextureIdx;
                uint32_t metalnessTextureIdx;
                uint32_t aoTextureIdx;
                uint32_t heightTextureIdx;
            };


        };


    }

}
