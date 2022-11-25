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

			Shader::Shader causticsShader;

			Shader::Shader shader;

			Shader::Uniform* nodeLocation = nullptr;
			Shader::Uniform* nodeSideLength = nullptr;

			Shader::Uniform* viewMatrix = nullptr;
			Shader::Uniform* inverseViewMatrix = nullptr;
			Shader::Uniform* projectionMatrix = nullptr;
			Shader::Uniform* inverseProjectionMatrix = nullptr;

			Shader::Uniform* cameraLocation = nullptr;

			Shader::Uniform* time = nullptr;

			Shader::Uniform* translation = nullptr;

			Shader::Uniform* displacementScale = nullptr;
			Shader::Uniform* choppyScale = nullptr;
			Shader::Uniform* tiling = nullptr;

			Shader::Uniform* shoreWaveDistanceOffset = nullptr;
			Shader::Uniform* shoreWaveDistanceScale = nullptr;
			Shader::Uniform* shoreWaveAmplitude = nullptr;
			Shader::Uniform* shoreWaveSteepness = nullptr;
			Shader::Uniform* shoreWavePower = nullptr;
			Shader::Uniform* shoreWaveSpeed = nullptr;
			Shader::Uniform* shoreWaveLength = nullptr;

			Shader::Uniform* leftLoD = nullptr;
			Shader::Uniform* topLoD = nullptr;
			Shader::Uniform* rightLoD = nullptr;
			Shader::Uniform* bottomLoD = nullptr;

			Shader::Uniform* lightDirection = nullptr;
			Shader::Uniform* lightColor = nullptr;
			Shader::Uniform* lightAmbient = nullptr;

			Shader::Uniform* shadowDistance = nullptr;
			Shader::Uniform* shadowBias = nullptr;
			Shader::Uniform* shadowCascadeCount = nullptr;
			Shader::Uniform* shadowResolution = nullptr;

			Shader::Uniform* terrainTranslation = nullptr;
			Shader::Uniform* terrainSideLength = nullptr;
			Shader::Uniform* terrainHeightScale = nullptr;

			Shader::Uniform* hasRippleTexture = nullptr;

			Shader::Uniform* fogScale = nullptr;
			Shader::Uniform* fogDistanceScale = nullptr;
			Shader::Uniform* fogHeight = nullptr;
			Shader::Uniform* fogColor = nullptr;
			Shader::Uniform* fogScatteringPower = nullptr;

			struct ShadowCascadeUniform {
				Shader::Uniform* distance = nullptr;
				Shader::Uniform* lightSpace = nullptr;
			}cascades[MAX_SHADOW_CASCADE_COUNT + 1];

			Shader::Uniform* pvMatrixLast = nullptr;
			Shader::Uniform* jitterLast = nullptr;
			Shader::Uniform* jitterCurrent = nullptr;

		};

	}

}


#endif