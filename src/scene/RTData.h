#ifndef AE_RTDATA_H
#define AE_RTDATA_H

#include "../System.h"

#include "../actor/MeshActor.h"
#include "../texture/TextureAtlas.h"

#include <vector>

namespace Atlas {

	// Forward-declare class to use it as a friend
	namespace Renderer::Helper {
		class RayTracingHelper;
	}

	namespace Scene {

		class Scene;

		class RTData {

			friend Renderer::Helper::RayTracingHelper;

		public:
			RTData() = default;

			RTData(Scene* scene);

			void Update();

			void UpdateMaterials( bool updateTextures = false);

			void UpdateTextures();

		private:
			struct GPUTriangle {
				vec4 v0;
				vec4 v1;
				vec4 v2;
				vec4 d0;
				vec4 d1;
			};

			struct BVHTriangle {
				vec4 v0;
				vec4 v1;
				vec4 v2;
				vec4 d0;
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

				float reflectance;

				float normalScale;

				int32_t invertUVs;
				int32_t twoSided;

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

			struct GPUBVHNode {
				glm::uvec4 data0;
				glm::uvec4 data1;
				glm::uvec4 data2;
				glm::uvec4 data3;
			};

			struct GPULight {
				vec4 data0;
				vec4 data1;
				vec4 N;
			};

			std::unordered_map<Material*, int32_t> UpdateMaterials(std::vector<GPUMaterial>& materials,
				bool updateTextures);

			Scene* scene;

			int32_t shaderStorageLimit;

			Buffer::Buffer triangleBuffer;
			Buffer::Buffer bvhTriangleBuffer;
			Buffer::Buffer materialBuffer;
			Buffer::Buffer nodeBuffer;

			Texture::TextureAtlas baseColorTextureAtlas;
			Texture::TextureAtlas opacityTextureAtlas;
			Texture::TextureAtlas normalTextureAtlas;
			Texture::TextureAtlas roughnessTextureAtlas;
			Texture::TextureAtlas metalnessTextureAtlas;
			Texture::TextureAtlas aoTextureAtlas;

			std::vector<GPULight> triangleLights;

		};

	}

}

#endif