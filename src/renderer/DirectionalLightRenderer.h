#ifndef AE_DIRECTIONALLIGHTRENDERER_H
#define AE_DIRECTIONALLIGHTRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

        class DirectionalLightRenderer : public Renderer {

        public:
            DirectionalLightRenderer();

            virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

            static std::string vertexPath;
            static std::string fragmentPath;

        private:
            void GetUniforms();

            Shader::Shader shader;

            Shader::Uniform* inverseViewMatrix = nullptr;
            Shader::Uniform* inverseProjectionMatrix = nullptr;

            Shader::Uniform* lightDirection = nullptr;
            Shader::Uniform* lightColor = nullptr;
            Shader::Uniform* lightAmbient = nullptr;

            Shader::Uniform* scatteringFactor = nullptr;

            Shader::Uniform* shadowDistance = nullptr;
            Shader::Uniform* shadowBias = nullptr;
            Shader::Uniform* shadowSampleCount = nullptr;
            Shader::Uniform* shadowSampleRange = nullptr;
            Shader::Uniform* shadowSampleRandomness = nullptr;
            Shader::Uniform* shadowCascadeCount = nullptr;
            Shader::Uniform* shadowResolution = nullptr;

            struct ShadowCascadeUniform {
                Shader::Uniform* distance;
                Shader::Uniform* lightSpace;
            }cascades[MAX_SHADOW_CASCADE_COUNT];

        };

	}

}

#endif