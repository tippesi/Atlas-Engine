#ifndef AE_OCEANRENDERER_H
#define AE_OCEANRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../ocean/OceanSimulation.h"

namespace Atlas {

	namespace Renderer {

		class OceanRenderer : public Renderer {

		public:
			OceanRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Texture::Texture2D refractionTexture;
			Texture::Texture2D depthTexture;

			OldShader::OldShader causticsShader;

			OldShader::OldShader shader;

			OldShader::Uniform* nodeLocation = nullptr;
			OldShader::Uniform* nodeSideLength = nullptr;

			OldShader::Uniform* viewMatrix = nullptr;
			OldShader::Uniform* inverseViewMatrix = nullptr;
			OldShader::Uniform* projectionMatrix = nullptr;
			OldShader::Uniform* inverseProjectionMatrix = nullptr;

			OldShader::Uniform* cameraLocation = nullptr;

			OldShader::Uniform* time = nullptr;

			OldShader::Uniform* translation = nullptr;

			OldShader::Uniform* displacementScale = nullptr;
			OldShader::Uniform* choppyScale = nullptr;
			OldShader::Uniform* tiling = nullptr;

			OldShader::Uniform* shoreWaveDistanceOffset = nullptr;
			OldShader::Uniform* shoreWaveDistanceScale = nullptr;
			OldShader::Uniform* shoreWaveAmplitude = nullptr;
			OldShader::Uniform* shoreWaveSteepness = nullptr;
			OldShader::Uniform* shoreWavePower = nullptr;
			OldShader::Uniform* shoreWaveSpeed = nullptr;
			OldShader::Uniform* shoreWaveLength = nullptr;

			OldShader::Uniform* leftLoD = nullptr;
			OldShader::Uniform* topLoD = nullptr;
			OldShader::Uniform* rightLoD = nullptr;
			OldShader::Uniform* bottomLoD = nullptr;

			OldShader::Uniform* lightDirection = nullptr;
			OldShader::Uniform* lightColor = nullptr;
			OldShader::Uniform* lightAmbient = nullptr;

			OldShader::Uniform* shadowDistance = nullptr;
			OldShader::Uniform* shadowBias = nullptr;
			OldShader::Uniform* shadowCascadeCount = nullptr;
			OldShader::Uniform* shadowResolution = nullptr;

			OldShader::Uniform* terrainTranslation = nullptr;
			OldShader::Uniform* terrainSideLength = nullptr;
			OldShader::Uniform* terrainHeightScale = nullptr;

			OldShader::Uniform* hasRippleTexture = nullptr;

			OldShader::Uniform* fogScale = nullptr;
			OldShader::Uniform* fogDistanceScale = nullptr;
			OldShader::Uniform* fogHeight = nullptr;
			OldShader::Uniform* fogColor = nullptr;
			OldShader::Uniform* fogScatteringPower = nullptr;

			struct ShadowCascadeUniform {
				OldShader::Uniform* distance = nullptr;
				OldShader::Uniform* lightSpace = nullptr;
			}cascades[MAX_SHADOW_CASCADE_COUNT + 1];

			OldShader::Uniform* pvMatrixLast = nullptr;
			OldShader::Uniform* jitterLast = nullptr;
			OldShader::Uniform* jitterCurrent = nullptr;

		};

	}

}


#endif