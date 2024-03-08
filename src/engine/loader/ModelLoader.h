#pragma once

#include "../System.h"
#include "../mesh/MeshData.h"
#include "../scene/Scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/types.h>

#include <set>
#include <mutex>

namespace Atlas {

    namespace Loader {

        class ModelLoader {

        public:
            static Ref<Mesh::Mesh> LoadMesh(const std::string& filename,
                bool forceTangents = false, int32_t maxTextureResolution = 4096);

            static Ref<Mesh::Mesh> LoadMesh(const std::string& filename,
                Mesh::MeshMobility mobility, bool forceTangents = false,
                int32_t maxTextureResolution = 4096);

            static Ref<Scene::Scene> LoadScene(const std::string& filename, vec3 min, vec3 max,
                int32_t depth, bool forceTangents = false, int32_t maxTextureResolution = 4096);

        private:
            enum class MaterialImageType {
                BaseColor = 0,
                Opacity,
                Roughness,
                Metallic,
                Normal,
                Displacement,
            };

            struct MaterialImages {
                std::map<std::string, Ref<Common::Image<uint8_t>>> baseColorImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> opacityImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> roughnessImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> metallicImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> normalImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> displacementImages;

                std::map<std::string, Ref<Texture::Texture2D>> baseColorTextures;
                std::map<std::string, Ref<Texture::Texture2D>> opacityTextures;
                std::map<std::string, Ref<Texture::Texture2D>> roughnessTextures;
                std::map<std::string, Ref<Texture::Texture2D>> metallicTextures;
                std::map<std::string, Ref<Texture::Texture2D>> normalTextures;
                std::map<std::string, Ref<Texture::Texture2D>> displacementTextures;

                std::mutex mutexes[6];

                void Add(MaterialImageType type, const std::string& path, const Ref<Common::Image<uint8_t>>& image) {
                    std::scoped_lock lock(mutexes[static_cast<int>(type)]);

                    switch (type) {
                    case MaterialImageType::BaseColor:
                        baseColorImages[path] = image;
                        break;
                    case MaterialImageType::Opacity:
                        opacityImages[path] = image;
                        break;
                    case MaterialImageType::Roughness:
                        roughnessImages[path] = image;
                        break;
                    case MaterialImageType::Metallic:
                        metallicImages[path] = image;
                        break;
                    case MaterialImageType::Normal:
                        normalImages[path] = image;
                        break;
                    case MaterialImageType::Displacement:
                        baseColorImages[path] = image;
                        break;
                    default:
                        break;
                    }
                }

                bool Contains(MaterialImageType type, const std::string& path) {
                    std::scoped_lock lock(mutexes[static_cast<int>(type)]);

                    switch (type) {
                    case MaterialImageType::BaseColor:
                        return baseColorImages.contains(path);
                    case MaterialImageType::Opacity:
                        return opacityImages.contains(path);
                    case MaterialImageType::Roughness:
                        return roughnessImages.contains(path);
                    case MaterialImageType::Metallic:
                        return metallicImages.contains(path);
                    case MaterialImageType::Normal:
                        return normalImages.contains(path);
                    case MaterialImageType::Displacement:
                        return displacementImages.contains(path);
                    default:
                        return true;
                    }
                }
            };

            static void LoadMaterial(aiMaterial* assimpMaterial, MaterialImages& images, Material& material,
                const std::string& directory, bool isObj, bool hasTangents, bool hasTexCoords);

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