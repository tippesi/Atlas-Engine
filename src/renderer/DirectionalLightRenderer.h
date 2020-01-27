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

        private:
            void GetUniforms();

            Shader::Shader shader;

            Shader::Uniform* inverseViewMatrix = nullptr;
            Shader::Uniform* inverseProjectionMatrix = nullptr;

			Shader::Uniform* cameraLocation = nullptr;

            Shader::Uniform* lightDirection = nullptr;
            Shader::Uniform* lightColor = nullptr;
            Shader::Uniform* lightAmbient = nullptr;

            Shader::Uniform* scatteringFactor = nullptr;

            Shader::Uniform* shadowDistance = nullptr;
            Shader::Uniform* shadowBias = nullptr;
			Shader::Uniform* shadowCascadeBlendDistance = nullptr;
            Shader::Uniform* shadowCascadeCount = nullptr;
            Shader::Uniform* shadowResolution = nullptr;

			Shader::Uniform* fogScale = nullptr;
			Shader::Uniform* fogDistanceScale = nullptr;
			Shader::Uniform* fogHeight = nullptr;
			Shader::Uniform* fogColor = nullptr;
			Shader::Uniform* fogScatteringPower = nullptr;

            struct ShadowCascadeUniform {
                Shader::Uniform* distance;
                Shader::Uniform* lightSpace;
				Shader::Uniform* texelSize;
            }cascades[MAX_SHADOW_CASCADE_COUNT + 1];

        };

	}

}

#endif