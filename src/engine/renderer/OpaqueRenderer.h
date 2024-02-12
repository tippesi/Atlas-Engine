#pragma once

#include "../System.h"
#include "../RenderList.h"

#include "Renderer.h"

#include <mutex>

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
                const ResourceHandle<Mesh::Mesh>& mesh, Ref<RenderTarget> target);

#ifndef AE_BINDLESS
            struct PushConstants {
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
            };
#else
            struct PushConstants {
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
                int baseColorMap;
                int opacityMap;
                int normalMap;
                int roughnessMap;
                int metalnessMap;
                int aoMap;
                int heightMap;
            };
#endif


        };


    }

}