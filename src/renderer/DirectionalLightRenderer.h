#ifndef AE_DIRECTIONALLIGHTRENDERER_H
#define AE_DIRECTIONALLIGHTRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

	namespace Renderer {

        class DirectionalLightRenderer : public Renderer {

        public:
            DirectionalLightRenderer();

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene) final {}

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
                Scene::Scene* scene, Texture::Texture2D* dfgTexture);

        private:
            void GetUniforms();

            Shader::Shader shader;

            Shader::Uniform* inverseViewMatrix = nullptr;
            Shader::Uniform* inverseProjectionMatrix = nullptr;

			Shader::Uniform* cameraLocation = nullptr;

            Shader::Uniform* lightDirection = nullptr;
            Shader::Uniform* lightColor = nullptr;
            Shader::Uniform* lightIntensity = nullptr;

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

            Shader::Uniform* volumeMin = nullptr;
            Shader::Uniform* volumeMax = nullptr;
            Shader::Uniform* volumeProbeCount = nullptr;
            Shader::Uniform* volumeIrradianceRes = nullptr;
            Shader::Uniform* volumeMomentsRes = nullptr;

            struct ShadowCascadeUniform {
                Shader::Uniform* distance = nullptr;
                Shader::Uniform* lightSpace = nullptr;
				Shader::Uniform* texelSize = nullptr;
            }cascades[MAX_SHADOW_CASCADE_COUNT + 1];

        };

	}

}

#endif