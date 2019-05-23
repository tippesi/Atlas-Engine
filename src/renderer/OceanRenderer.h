#ifndef AE_OCEANRENDERER_H
#define AE_OCEANRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "gpgpu/OceanSimulation.h"

namespace Atlas {

	namespace Renderer {

		class OceanRenderer : public Renderer {

		public:
			OceanRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Update();

			static std::string vertexPath;
			static std::string fragmentPath;

			GPGPU::OceanSimulation* simulation;

		private:
			void GetUniforms();

			Buffer::VertexArray vertexArray;

			Texture::Texture2D foam;
			Texture::Texture2D refractionTexture;
			Texture::Texture2D depthTexture;

			Shader::Shader shader;

			Shader::Uniform* viewMatrix = nullptr;
			Shader::Uniform* inverseViewMatrix = nullptr;
			Shader::Uniform* projectionMatrix = nullptr;
			Shader::Uniform* inverseProjectionMatrix = nullptr;

			Shader::Uniform* cameraLocation = nullptr;

			Shader::Uniform* displacementScale = nullptr;
			Shader::Uniform* choppyScale = nullptr;

			Shader::Uniform* lightDirection = nullptr;
			Shader::Uniform* lightColor = nullptr;
			Shader::Uniform* lightAmbient = nullptr;
			Shader::Uniform* lightScatteringFactor = nullptr;

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