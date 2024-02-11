#pragma once

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
            static Ref<Mesh::Mesh> LoadMesh(const std::string& filename,
                bool forceTangents = false, int32_t maxTextureResolution = 4096);

            static Ref<Mesh::Mesh> LoadMesh(const std::string& filename,
                Mesh::MeshMobility mobility, bool forceTangents = false,
                int32_t maxTextureResolution = 4096);

            static Ref<Scene::Scene> LoadScene(const std::string& filename,
                bool forceTangents = false, int32_t maxTextureResolution = 4096);

        private:
            struct MaterialImages {
                Ref<Common::Image<uint8_t>> baseColorImage;
                Ref<Common::Image<uint8_t>> opacityImage;
                Ref<Common::Image<uint8_t>> roughnessImage;
                Ref<Common::Image<uint8_t>> metallicImage;
                Ref<Common::Image<uint8_t>> normalImage;
                Ref<Common::Image<uint8_t>> displacementImage;

                Ref<Texture::Texture2D> baseColorTexture;
                Ref<Texture::Texture2D> opacityTexture;
                Ref<Texture::Texture2D> roughnessTexture;
                Ref<Texture::Texture2D> metallicTexture;
                Ref<Texture::Texture2D> normalTexture;
                Ref<Texture::Texture2D> displacementTexture;
            };

            static void LoadMaterial(aiMaterial* assimpMaterial, MaterialImages& images, Material& material);

            static void LoadMaterialImages(aiMaterial* material, MaterialImages& images,
                const std::string& directory, bool isObj, bool hasTangents,
                int32_t maxTextureResolution, bool rgbSupport);

            static void ImagesToTexture(MaterialImages& images);

            static std::string GetDirectoryPath(std::string filename);

            template<typename T>
            static Ref<Common::Image<T>> ApplySobelFilter(const Ref<Common::Image<T>>& image, const float strength = 1.0f) {

                auto filtered = CreateRef<Common::Image<T>>(image->width, image->height, 3);

                auto size = image->width * image->height;

                for (int32_t i = 0; i < size; i++) {

                    auto x = i % image->width;
                    auto y = i / image->width;

                    auto maxValue = image->MaxPixelValue();

                    auto tl = float(image->Sample(x - 1, y - 1).r) / maxValue;
                    auto tc = float(image->Sample(x, y - 1).r) / maxValue;
                    auto tr = float(image->Sample(x + 1, y - 1).r) / maxValue;
                    auto ml = float(image->Sample(x - 1, y).r) / maxValue;
                    auto mr = float(image->Sample(x + 1, y).r) / maxValue;
                    auto bl = float(image->Sample(x - 1, y + 1).r) / maxValue;
                    auto bc = float(image->Sample(x, y + 1).r) / maxValue;
                    auto br = float(image->Sample(x + 1, y + 1).r) / maxValue;

                    auto dx = (tl + 2.0f * ml + bl) - (tr + 2.0f * mr + br);
                    auto dy = (tl + 2.0f * tc + tr) - (bl + 2.0f * bc + br);
                    auto dz = 1.0f / strength;

                    auto center = (0.5f * glm::normalize(vec3(dx, dy, dz)) + 0.5f) * maxValue;

                    filtered->SetData(x, y, 0, T(center.r));
                    filtered->SetData(x, y, 1, T(center.g));
                    filtered->SetData(x, y, 2, T(center.b));

                }

                return filtered;

            }

        };

    }

}