#pragma once

#include "../System.h"
#include "Renderer.h"
#include "helper/VegetationHelper.h"

namespace Atlas {

    namespace Renderer {

        class VegetationRenderer : public Renderer {

        public:
            VegetationRenderer();

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                std::unordered_map<void*, uint16_t> materialMap);

            Helper::VegetationHelper helper;

        private:
            PipelineConfig GetPipelineConfigForSubData(Mesh::MeshSubData* subData,
                ResourceHandle<Mesh::Mesh>& mesh, Ref<RenderTarget> target);

            void DepthPrepass(Scene::Vegetation& vegetation, std::vector<Mesh::Mesh*>& meshes,
                const CameraComponent& camera, float time, float deltaTime);

        };

    }

}