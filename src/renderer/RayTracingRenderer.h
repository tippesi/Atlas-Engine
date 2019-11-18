#ifndef AE_GPURAYTRACINGRENDERER_H
#define AE_GPURAYTRACINGRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		class RayTracingRenderer : public Renderer {

		public:
			RayTracingRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Viewport* viewport, Texture::Texture2D* texture, Texture::Texture2D* inAccumTexture,
				Texture::Texture2D* outAccumTexture, ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene);

			bool UpdateData(Scene::Scene* scene);

			int32_t GetSampleCount();

			static std::string vertexUpdateComputePath;
			static std::string BVHComputePath;
			static std::string rayCasterComputePath;

		private:
			void GetVertexUpdateUniforms();
			void GetBVHUniforms();
			void GetRayCasterUniforms();

			void UpdateTexture(Scene::Scene* scene);

			struct Triangle {
				vec3 v0;
				vec3 v1;
				vec3 v2;
				vec3 n0;
				vec3 n1;
				vec3 n2;
				vec2 t0;
				vec2 t1;
				vec2 t2;
				int32_t materialIndex;
			};

			struct GPUTriangle {
				vec4 v0;
				vec4 v1;
				vec4 v2;
				vec4 n0;
				vec4 n1;
				vec4 n2;
			};

			struct GPUMaterial {
				vec3 diffuseColor;
				vec3 emissiveColor;

				float specularIntensity;
				float specularHardness;

				int32_t textureLayer;
				int32_t textureWidth;
				int32_t textureHeight;
			};

			struct GPUAABB {
				vec3 min;
				vec3 max;
			};

			// Cache friendly 32 bit
			struct GPUBVHNode {
				union {
					struct {
						uint32_t leftChild;
						uint32_t rightChild;
					}inner;
					struct {
						uint32_t dataOffset;
						uint32_t dataCount;
					}leaf;
				};				
				GPUAABB aabb;
			};

			int32_t workGroupLimit;
			int32_t shaderStorageLimit;
			int32_t textureUnitCount;

			vec3 cameraLocation;
			vec2 cameraRotation;

			int32_t sampleCount = 0;
			ivec2 imageOffset = ivec2(0);

			Buffer::Buffer triangleBuffer;
			Buffer::Buffer materialBuffer;
			Buffer::Buffer materialIndicesBuffer;
			Buffer::Buffer nodesBuffer;

			Texture::Texture2DArray textureArray;

			Shader::Shader vertexUpdateShader;

			Shader::Uniform* modelMatrixVertexUpdateUniform = nullptr;
			Shader::Uniform* triangleOffsetVertexUpdateUniform = nullptr;
			Shader::Uniform* triangleCountVertexUpdateUniform = nullptr;
			Shader::Uniform* xInvocationsVertexUpdateUniform = nullptr;

			Shader::Shader BVHShader;

			Shader::Shader rayCasterShader;

			Shader::Uniform* widthRayCasterUniform = nullptr;
			Shader::Uniform* heightRayCasterUniform = nullptr;
			Shader::Uniform* originRayCasterUniform = nullptr;
			Shader::Uniform* rightRayCasterUniform = nullptr;
			Shader::Uniform* bottomRayCasterUniform = nullptr;
			Shader::Uniform* cameraLocationRayCasterUniform = nullptr;
			Shader::Uniform* cameraFarPlaneRayCasterUniform = nullptr;
			Shader::Uniform* cameraNearPlaneRayCasterUniform = nullptr;
			Shader::Uniform* triangleCountRayCasterUniform = nullptr;
			Shader::Uniform* lightDirectionRayCasterUniform = nullptr;
			Shader::Uniform* lightColorRayCasterUniform = nullptr;
			Shader::Uniform* lightAmbientRayCasterUniform = nullptr;
			Shader::Uniform* sampleCountRayCasterUniform = nullptr;
			Shader::Uniform* pixelOffsetRayCasterUniform = nullptr;

		};

	}

}

#endif