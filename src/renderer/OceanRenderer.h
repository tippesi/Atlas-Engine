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

			Shader::Uniform* viewMatrix;
			Shader::Uniform* inverseViewMatrix;
			Shader::Uniform* projectionMatrix;
			Shader::Uniform* inverseProjectionMatrix;

			Shader::Uniform* cameraLocation;

			Shader::Uniform* displacementScale;
			Shader::Uniform* choppyScale;

			Shader::Uniform* lightDirection;
			Shader::Uniform* lightColor;
			Shader::Uniform* lightAmbient;
			Shader::Uniform* lightScatteringFactor;

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