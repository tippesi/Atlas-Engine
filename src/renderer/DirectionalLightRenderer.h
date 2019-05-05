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

            Shader::Uniform* diffuseTexture;
            Shader::Uniform* normalTexture;
            Shader::Uniform* materialTexture;
            Shader::Uniform* depthTexture;
            Shader::Uniform* aoTexture;
            Shader::Uniform* volumetricTexture;
            Shader::Uniform* shadowTexture;

            Shader::Uniform* inverseViewMatrix;
            Shader::Uniform* inverseProjectionMatrix;

            Shader::Uniform* lightDirection;
            Shader::Uniform* lightColor;
            Shader::Uniform* lightAmbient;

            Shader::Uniform* scatteringFactor;

            Shader::Uniform* shadowDistance;
            Shader::Uniform* shadowBias;
            Shader::Uniform* shadowSampleCount;
            Shader::Uniform* shadowSampleRange;
            Shader::Uniform* shadowSampleRandomness;
            Shader::Uniform* shadowCascadeCount;
            Shader::Uniform* shadowResolution;

            struct ShadowCascadeUniform {
                Shader::Uniform* distance;
                Shader::Uniform* lightSpace;
            }cascades[MAX_SHADOW_CASCADE_COUNT];

        };

	}

}

#endif