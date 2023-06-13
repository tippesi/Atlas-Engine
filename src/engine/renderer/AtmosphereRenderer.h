#ifndef AE_ATMOSPHERERENDERER_H
#define AE_ATMOSPHERERENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../lighting/EnvironmentProbe.h"

namespace Atlas {

    namespace Renderer {


        class AtmosphereRenderer : public Renderer {

        public:
            AtmosphereRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Graphics::CommandList* commandList);

            void Render(Lighting::EnvironmentProbe* probe, Scene::Scene* scene,
                Graphics::CommandList* commandList);

            static std::string vertexPath;
            static std::string fragmentPath;

        private:
            struct alignas(16) Uniforms {
                mat4 ivMatrix;
                mat4 ipMatrix;
                vec4 cameraLocation;
                vec4 planetCenter;
                vec4 sunDirection;
                float sunIntensity;
                float planetRadius;
                float atmosphereRadius;
            };

            PipelineConfig defaultPipelineConfig;
            PipelineConfig cubeMapPipelineConfig;

            Buffer::UniformBuffer uniformBuffer;
            Buffer::UniformBuffer probeMatricesBuffer;

        };

    }

}

#endif
