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

        class ModelImporter {

        public:
            static Ref<Mesh::Mesh> ImportMesh(const std::string& filename,
                bool saveToDisk = false, int32_t maxTextureResolution = 4096);

            static Ref<Mesh::Mesh> ImportMesh(const std::string& filename,
                Mesh::MeshMobility mobility, bool saveToDisk = false,
                int32_t maxTextureResolution = 4096);

            static Ref<Scene::Scene> ImportScene(const std::string& filename, vec3 min, vec3 max,
                int32_t depth, bool saveToDisk = true, bool makeMeshesStatic = false,
                bool invertUVs = false, int32_t maxTextureResolution = 4096);

        private:
            struct Paths {
                std::string filename;
                std::string directoryPath;
                std::string meshPath;
                std::string materialPath;
                std::string texturePath;
            };

            enum class MaterialImageType {
                BaseColor = 0,
                Opacity,
                Roughness,
                Metallic,
                Normal,
                Displacement,
                Emissive,
                Count
            };

            struct MaterialImages {
                std::map<std::string, Ref<Common::Image<uint8_t>>> baseColorImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> opacityImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> roughnessImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> metallicImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> normalImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> displacementImages;
                std::map<std::string, Ref<Common::Image<uint8_t>>> emissiveImages;

                std::map<std::string, ResourceHandle<Texture::Texture2D>> baseColorTextures;
                std::map<std::string, ResourceHandle<Texture::Texture2D>> opacityTextures;
                std::map<std::string, ResourceHandle<Texture::Texture2D>> roughnessTextures;
                std::map<std::string, ResourceHandle<Texture::Texture2D>> metallicTextures;
                std::map<std::string, ResourceHandle<Texture::Texture2D>> normalTextures;
                std::map<std::string, ResourceHandle<Texture::Texture2D>> displacementTextures;
                std::map<std::string, ResourceHandle<Texture::Texture2D>> emissiveTextures;

                std::mutex mutexes[static_cast<int>(MaterialImageType::Count)];

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
                        displacementImages[path] = image;
                        break;
                    case MaterialImageType::Emissive:
                        emissiveImages[path] = image;
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
                    case MaterialImageType::Emissive:
                        return emissiveImages.contains(path);
                    default:
                        return true;
                    }
                }
            };

            struct ImporterState {
                Paths paths;
                MaterialImages images;

                Assimp::Importer importer;
                const aiScene* scene;

                bool isObj;
            };

            static void InitImporterState(ImporterState& state, const std::string& filename, bool optimizeMeshes);

            static std::vector<ResourceHandle<Material>> ImportMaterials(ImporterState& state, int32_t maxTextureResolution, bool saveToDisk);

            static void LoadMaterial(ImporterState& state, aiMaterial* assimpMaterial, Material& material);

            static void LoadMaterialImages(ImporterState& state, aiMaterial* material, bool hasTangents,
                int32_t maxTextureResolution);

            static std::vector<Ref<Common::Image<uint8_t>>> ImagesToTextures(ImporterState& state);

            static Paths GetPaths(const std::string& filename);

            static std::string GetMaterialImageImportPath(const ImporterState& state, MaterialImageType type, const std::string& filename);

            static bool IsImageValid(Ref<Common::Image<uint8_t>>& image);

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