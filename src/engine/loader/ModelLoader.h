#ifndef AE_MODELLOADER_H
#define AE_MODELLOADER_H

#include "../System.h"
#include "../mesh/MeshData.h"
#include "../scene/Scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/types.h>

namespace Atlas {

	namespace Loader {

		class ModelLoader {

		public:
			static Mesh::MeshData LoadMesh(std::string filename,
				bool forceTangents = false, mat4 transform = mat4(1.0f),
				int32_t maxTextureResolution = 4096);

		private:
			struct MaterialImages {
				Common::Image<uint8_t> baseColorImage;
				Common::Image<uint8_t> opacityImage;
				Common::Image<uint8_t> roughnessImage;
				Common::Image<uint8_t> metallicImage;
				Common::Image<uint8_t> normalImage;
				Common::Image<uint8_t> displacementImage;
			};

			static void LoadMaterial(aiMaterial* assimpMaterial, MaterialImages& images, Material& material);

			static void LoadMaterialImages(aiMaterial* material, MaterialImages& images,
				const std::string& directory, bool isObj, bool hasTangents,
                int32_t maxTextureResolution, bool rgbSupport);

            static std::string GetDirectoryPath(std::string filename);

			template<typename T>
			static Common::Image<T> ApplySobelFilter(const Common::Image<T>& image, const float strength = 1.0f) {

				Common::Image<T> filtered(image.width, image.height, 3);

				auto size = image.width * image.height;

				for (int32_t i = 0; i < size; i++) {

					auto x = i % image.width;
					auto y = i / image.width;

					auto maxValue = image.MaxPixelValue();

					auto tl = float(image.Sample(x - 1, y - 1).r) / maxValue;
					auto tc = float(image.Sample(x, y - 1).r) / maxValue;
					auto tr = float(image.Sample(x + 1, y - 1).r) / maxValue;
					auto ml = float(image.Sample(x - 1, y).r) / maxValue;
					auto mr = float(image.Sample(x + 1, y).r) / maxValue;
					auto bl = float(image.Sample(x - 1, y + 1).r) / maxValue;
					auto bc = float(image.Sample(x, y + 1).r) / maxValue;
					auto br = float(image.Sample(x + 1, y + 1).r) / maxValue;

					auto dx = (tl + 2.0f * ml + bl) - (tr + 2.0f * mr + br);
					auto dy = (tl + 2.0f * tc + tr) - (bl + 2.0f * bc + br);
					auto dz = 1.0f / strength;

					auto center = (0.5f * glm::normalize(vec3(dx, dy, dz)) + 0.5f) * maxValue;

					filtered.SetData(x, y, 0, T(center.r));
					filtered.SetData(x, y, 1, T(center.g));
					filtered.SetData(x, y, 2, T(center.b));

				}

				return filtered;

			}

		};

	}

}

#endif