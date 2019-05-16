#ifndef AE_GPURAYTRACINGRENDERER_H
#define AE_GPURAYTRACINGRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		class GPURayTracingRenderer : public Renderer {

		public:
			GPURayTracingRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Viewport* viewport, Texture::Texture2D* texture, Camera* camera, Scene::Scene* scene);

			static std::string vertexUpdateComputePath;
			static std::string BVHComputePath;
			static std::string rayCasterComputePath;

		private:
			void GetVertexUpdateUniforms();
			void GetBVHUniforms();
			void GetRayCasterUniforms();

			bool UpdateGPUData(Scene::Scene* scene);

			struct GPUTriangle {
				vec4 v0;
				vec4 v1;
				vec4 v2;
			};

			int32_t workGroupLimit;
			int32_t shaderStorageLimit;

			Buffer::Buffer triangleBuffer;

			Shader::Shader vertexUpdateShader;

			Shader::Uniform* modelMatrixVertexUpdateUniform;
			Shader::Uniform* triangleOffsetVertexUpdateUniform;
			Shader::Uniform* triangleCountVertexUpdateUniform;
			Shader::Uniform* xInvocationsVertexUpdateUniform;

			Shader::Shader BVHShader;


			Shader::Shader rayCasterShader;

			Shader::Uniform* widthRayCasterUniform;
			Shader::Uniform* heightRayCasterUniform;
			Shader::Uniform* originRayCasterUniform;
			Shader::Uniform* rightRayCasterUniform;
			Shader::Uniform* bottomRayCasterUniform;
			Shader::Uniform* cameraLocationRayCasterUniform;
			Shader::Uniform* cameraFarPlaneRayCasterUniform;
			Shader::Uniform* cameraNearPlaneRayCasterUniform;
			Shader::Uniform* triangleCountRayCasterUniform;

		};

	}

}

#endif