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

				rayBuffer0 = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, 3 * sizeof(vec4), 0);
				rayBuffer1 = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, 3 * sizeof(vec4), 0);

				rayBuffer0.SetSize(size_t(width) * size_t(height));
				rayBuffer1.SetSize(size_t(width) * size_t(height));

			}

			void Resize(int32_t width, int32_t height) {
				this->width = width;
				this->height = height;

				texture.Resize(width, height);

				accumTexture0.Resize(width, height);
				accumTexture1.Resize(width, height);

				rayBuffer0.SetSize(size_t(width) * size_t(height));
				rayBuffer1.SetSize(size_t(width) * size_t(height));

			}

			int32_t GetWidth() const { return width; }
			int32_t GetHeight() const { return height; }

			Texture::Texture2D texture;

			Texture::Texture2D accumTexture0;
			Texture::Texture2D accumTexture1;

			Buffer::Buffer rayBuffer0;
			Buffer::Buffer rayBuffer1;

			int32_t sampleCount = 0;

		private:
			int32_t width = 0;
			int32_t height = 0;

		};

		class RayTracingRenderer : public Renderer {

		public:
			RayTracingRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

			void Render(Viewport* viewport, RayTracerRenderTarget* renderTarget,
				ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene);

			bool UpdateData(Scene::Scene* scene);

			void UpdateMaterials(Scene::Scene* scene);

			void ResetSampleCount();

			int32_t GetSampleCount() const;

			int32_t bounces = 10;
			int32_t bvhDepth = 0;
			int32_t lightCount = 512;

		private:
			struct Triangle {
				vec3 v0;
				vec3 v1;
				vec3 v2;

				vec3 n0;
				vec3 n1;
				vec3 n2;

				vec2 uv0;
				vec2 uv1;
				vec2 uv2;

				int32_t materialIdx;

				Triangle Subdivide(vec2 b0, vec2 b1, vec2 b2) {

					Triangle sub;

					sub.v0 = (1.0f - b0.x - b0.y) * v0 + b0.x * v1 + b0.y * v2;
					sub.v1 = (1.0f - b1.x - b1.y) * v0 + b1.x * v1 + b1.y * v2;
					sub.v2 = (1.0f - b2.x - b2.y) * v0 + b2.x * v1 + b2.y * v2;

					sub.n0 = (1.0f - b0.x - b0.y) * n0 + b0.x * n1 + b0.y * n2;
					sub.n1 = (1.0f - b1.x - b1.y) * n0 + b1.x * n1 + b1.y * n2;
					sub.n2 = (1.0f - b2.x - b2.y) * n0 + b2.x * n1 + b2.y * n2;

					sub.uv0 = (1.0f - b0.x - b0.y) * uv0 + b0.x * uv1 + b0.y * uv2;
					sub.uv1 = (1.0f - b1.x - b1.y) * uv0 + b1.x * uv1 + b1.y * uv2;
					sub.uv2 = (1.0f - b2.x - b2.y) * uv0 + b2.x * uv1 + b2.y * uv2;

					sub.materialIdx = materialIdx;

					return sub;

				}
			};

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
				vec3 baseColor;
				vec3 emissiveColor;

				float opacity;

				float roughness;
				float metalness;
				float ao;

				float normalScale;

				int32_t invertUVs;

				GPUTexture baseColorTexture;
				GPUTexture opacityTexture;
				GPUTexture normalTexture;
				GPUTexture roughnessTexture;
				GPUTexture metalnessTexture;
				GPUTexture aoTexture;
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

			struct GPULight {
				vec4 data0;
				vec4 data1;
				vec4 N;
			};

			void GetPrimaryRayUniforms();
			void GetBounceUpdateUniforms();
			void GetRayUpdateUniforms();

			void UpdateTexture(Scene::Scene* scene);

			std::unordered_map<Material*, int32_t> UpdateMaterials(Scene::Scene* scene,
				std::vector<GPUMaterial>& gpuMaterials);

			void CutTriangles(std::vector<Volume::AABB>& aabbs, std::vector<Triangle>& triangles);

			int32_t workGroupLimit;
			int32_t shaderStorageLimit;
			int32_t textureUnitCount;

			vec3 cameraLocation;
			vec2 cameraRotation;

			int32_t sampleCount = 0;
			ivec2 imageOffset = ivec2(0);

			std::vector<GPULight> lights;

			Buffer::Buffer indirectSSBOBuffer;

			Buffer::Buffer indirectDispatchBuffer;
			Buffer::Buffer counterBuffer0;
			Buffer::Buffer counterBuffer1;

			Buffer::Buffer triangleBuffer;
			Buffer::Buffer materialBuffer;
			Buffer::Buffer nodeBuffer;
			Buffer::Buffer lightBuffer;

			Texture::TextureAtlas baseColorTextureAtlas;
			Texture::TextureAtlas opacityTextureAtlas;
			Texture::TextureAtlas normalTextureAtlas;
			Texture::TextureAtlas roughnessTextureAtlas;
			Texture::TextureAtlas metalnessTextureAtlas;
			Texture::TextureAtlas aoTextureAtlas;

			Shader::Shader primaryRayShader;

			Shader::Uniform* cameraLocationPrimaryRayUniform = nullptr;
			Shader::Uniform* originPrimaryRayUniform = nullptr;
			Shader::Uniform* rightPrimaryRayUniform = nullptr;
			Shader::Uniform* bottomPrimaryRayUniform = nullptr;

			Shader::Uniform* sampleCountPrimaryRayUniform = nullptr;
			Shader::Uniform* pixelOffsetPrimaryRayUniform = nullptr;

			Shader::Uniform* tileSizePrimaryRayUniform = nullptr;
			Shader::Uniform* resolutionPrimaryRayUniform = nullptr;

			Shader::Shader bounceDispatchShader;

			Shader::Shader bounceUpdateShader;

			Shader::Uniform* maxBouncesBounceUpdateUniform = nullptr;

			Shader::Uniform* sampleCountBounceUpdateUniform = nullptr;
			Shader::Uniform* bounceCountBounceUpdateUniform = nullptr;
			Shader::Uniform* lightCountBounceUpdateUniform = nullptr;

			Shader::Uniform* resolutionBounceUpdateUniform = nullptr;

			Shader::Uniform* seedBounceUpdateUniform = nullptr;

		};

	}

}

#endif