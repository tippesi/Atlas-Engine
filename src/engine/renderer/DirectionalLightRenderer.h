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
                Scene::Scene* scene);

        private:
            void GetUniforms();

            OldShader::OldShader shader;

            OldShader::Uniform* inverseViewMatrix = nullptr;
            OldShader::Uniform* inverseProjectionMatrix = nullptr;

			OldShader::Uniform* cameraLocation = nullptr;

            OldShader::Uniform* lightDirection = nullptr;
            OldShader::Uniform* lightColor = nullptr;
            OldShader::Uniform* lightIntensity = nullptr;

            OldShader::Uniform* scatteringFactor = nullptr;

            OldShader::Uniform* shadowDistance = nullptr;
            OldShader::Uniform* shadowBias = nullptr;
			OldShader::Uniform* shadowCascadeBlendDistance = nullptr;
            OldShader::Uniform* shadowCascadeCount = nullptr;
            OldShader::Uniform* shadowResolution = nullptr;

			OldShader::Uniform* fogScale = nullptr;
			OldShader::Uniform* fogDistanceScale = nullptr;
			OldShader::Uniform* fogHeight = nullptr;
			OldShader::Uniform* fogColor = nullptr;
			OldShader::Uniform* fogScatteringPower = nullptr;

            OldShader::Uniform* volumeMin = nullptr;
            OldShader::Uniform* volumeMax = nullptr;
            OldShader::Uniform* volumeProbeCount = nullptr;
            OldShader::Uniform* volumeIrradianceRes = nullptr;
            OldShader::Uniform* volumeMomentsRes = nullptr;

            struct ShadowCascadeUniform {
                OldShader::Uniform* distance = nullptr;
                OldShader::Uniform* lightSpace = nullptr;
				OldShader::Uniform* texelSize = nullptr;
            }cascades[MAX_SHADOW_CASCADE_COUNT + 1];

        };

	}

}

#endif