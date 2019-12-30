#ifndef AE_GPURAYTRACINGRENDERER_H
#define AE_GPURAYTRACINGRENDERER_H

#include "../System.h"
#include "Renderer.h"

#include "../texture/TextureAtlas.h"

#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		class RayTracerRenderTarget {

		public:
			RayTracerRenderTarget() {}

			RayTracerRenderTarget(int32_t width, int32_t height) : width(width), height(height) {
				texture = Texture::Texture2D(width, height, AE_RGBA8,
					GL_CLAMP_TO_EDGE, GL_LINEAR);

				accumTexture0 = Texture::Texture2D(width, height, AE_RGBA32F);
				accumTexture1 = Texture::Texture2D(width, height, AE_RGBA32F);
			}

			void Resize(int32_t width, int32_t height) {
				this->width = width;
				this->height = height;

				texture.Resize(width, height);

				accumTexture0.Resize(width, height);
				accumTexture1.Resize(width, height);
			}

			int32_t GetWidth() { return width; }
			int32_t GetHeight() { return height; }

			Texture::Texture2D texture;

			Texture::Texture2D accumTexture0;
			Texture::Texture2D accumTexture1;

		private:
			int32_t width = 0;
			int32_t height = 0;

		};

		class RayTracingRenderer : public Renderer {

		public:
			RayTracingRenderer();

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Viewport* viewport, RayTracerRenderTarget* renderTarget,
				ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene);

			bool UpdateData(Scene::Scene* scene);

			std::unordered_map<Material*, int32_t> UpdateMaterials(Scene::Scene* scene);

			void ResetSampleCount();

			int32_t GetSampleCount();

			static std::string vertexUpdateComputePath;
			static std::string BVHComputePath;
			static std::string rayCasterComputePath;

		private:
			void GetVertexUpdateUniforms();
			void GetBVHUniforms();
			void GetRayCasterUniforms();

			void UpdateTexture(Scene::Scene* scene);

			int32_t PackUnitVector(vec4 vector);

			struct GPUTriangle {
				vec4 v0;
				vec4 v1;
				vec4 v2;
				vec4 d0;
				vec4 d1;
			};

			struct GPUTexture {

				int32_t layer;

				int32_t x;
				int32_t y;

				int32_t width;
				int32_t height;

			};

			struct GPUMaterial {
				vec3 diffuseColor;
				vec3 emissiveColor;

				float specularIntensity;
				float specularHardness;

				float normalScale;

				GPUTexture diffuseTexture;
				GPUTexture normalTexture;
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
			Buffer::Buffer nodesBuffer;

			Texture::TextureAtlas diffuseTextureAtlas;
			Texture::TextureAtlas normalTextureAtlas;

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