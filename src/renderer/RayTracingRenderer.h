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

				rayBuffer0.SetSize(width * height);
				rayBuffer1.SetSize(width * height);

			}

			void Resize(int32_t width, int32_t height) {
				this->width = width;
				this->height = height;

				texture.Resize(width, height);

				accumTexture0.Resize(width, height);
				accumTexture1.Resize(width, height);

				rayBuffer0.SetSize(width * height);
				rayBuffer1.SetSize(width * height);

			}

			int32_t GetWidth() { return width; }
			int32_t GetHeight() { return height; }

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

			virtual void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

			void Render(Viewport* viewport, RayTracerRenderTarget* renderTarget,
				ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene);

			bool UpdateData(Scene::Scene* scene);

			std::unordered_map<Material*, int32_t> UpdateMaterials(Scene::Scene* scene);

			void ResetSampleCount();

			int32_t GetSampleCount();

			static std::string primaryRayComputePath;
			static std::string bounceUpdateComputePath;
			static std::string rayUpdateComputePath;

			int32_t bounces = 2;

		private:
			void GetPrimaryRayUniforms();
			void GetBounceUpdateUniforms();
			void GetRayUpdateUniforms();

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

				float opacity;

				float specularIntensity;
				float specularHardness;

				float normalScale;

				int32_t invertUVs;

				GPUTexture diffuseTexture;
				GPUTexture normalTexture;
				GPUTexture specularTexture;
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

			Buffer::Buffer indirectSSBOBuffer;

			Buffer::Buffer indirectDispatchBuffer;
			Buffer::Buffer counterBuffer0;
			Buffer::Buffer counterBuffer1;

			Buffer::Buffer triangleBuffer;
			Buffer::Buffer materialBuffer;
			Buffer::Buffer nodesBuffer;

			Texture::TextureAtlas diffuseTextureAtlas;
			Texture::TextureAtlas normalTextureAtlas;
			Texture::TextureAtlas specularTextureAtlas;

			Shader::Shader primaryRayShader;

			Shader::Uniform* cameraLocationPrimaryRayUniform = nullptr;
			Shader::Uniform* originPrimaryRayUniform = nullptr;
			Shader::Uniform* rightPrimaryRayUniform = nullptr;
			Shader::Uniform* bottomPrimaryRayUniform = nullptr;

			Shader::Uniform* sampleCountPrimaryRayUniform = nullptr;
			Shader::Uniform* pixelOffsetPrimaryRayUniform = nullptr;

			Shader::Uniform* tileSizePrimaryRayUniform = nullptr;
			Shader::Uniform* resolutionPrimaryRayUniform = nullptr;

			Shader::Shader bounceUpdateShader;

			Shader::Shader rayUpdateShader;

			Shader::Uniform* cameraLocationRayUpdateUniform = nullptr;
			
			Shader::Uniform* lightDirectionRayUpdateUniform = nullptr;
			Shader::Uniform* lightColorRayUpdateUniform = nullptr;
			Shader::Uniform* lightAmbientRayUpdateUniform = nullptr;

			Shader::Uniform* sampleCountRayUpdateUniform = nullptr;
			Shader::Uniform* bounceCountRayUpdateUniform = nullptr;

			Shader::Uniform* resolutionRayUpdateUniform = nullptr;

			Shader::Uniform* seedRayUpdateUniform = nullptr;

		};

	}

}

#endif