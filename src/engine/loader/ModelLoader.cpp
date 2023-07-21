#include "ModelLoader.h"
#include "ImageLoader.h"
#include "AssetLoader.h"
#include "../Log.h"
#include "../common/Path.h"
#include "../graphics/Instance.h"

#include <vector>
#include <limits>
#include <functional>
#include <thread>
#include <atomic>

namespace Atlas {

    namespace Loader {

        Ref<Mesh::MeshData> ModelLoader::LoadMesh(const std::string& filename,
            bool forceTangents, mat4 transform, int32_t maxTextureResolution) {

            Mesh::MeshData meshData;

            auto directoryPath = GetDirectoryPath(filename);

            AssetLoader::UnpackFile(filename);

            auto fileType = Common::Path::GetFileType(filename);
            bool isObj = fileType == "obj" || fileType == "OBJ";

            Assimp::Importer importer;
            importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);

            // Use aiProcess_GenSmoothNormals in case model lacks normals and smooth normals are needed
            // Use aiProcess_GenNormals in case model lacks normals and flat normals are needed
            // Right now we just use flat normals everytime normals are missing.
            const aiScene* scene = importer.ReadFile(AssetLoader::GetFullPath(filename),
                                                     aiProcess_CalcTangentSpace |
                                                     aiProcess_JoinIdenticalVertices |
                                                     aiProcess_Triangulate |
                                                     aiProcess_OptimizeGraph |
                                                     aiProcess_OptimizeMeshes |
                                                     aiProcess_RemoveRedundantMaterials |
                                                     aiProcess_GenNormals |
                                                     aiProcess_LimitBoneWeights |
                                                     aiProcess_ImproveCacheLocality);

            if (!scene) {
                Log::Error("Error processing model " + std::string(importer.GetErrorString()));
            }

            int32_t indexCount = 0;
            int32_t vertexCount = 0;
            int32_t bonesCount = 0;

            bool hasTangents = false;
            bool hasTexCoords = false;

            struct AssimpMesh {
                aiMesh* mesh;
                mat4 transform;
            };

            std::vector<std::vector<AssimpMesh>> meshSorted(scene->mNumMaterials);

            std::function<void(aiNode*, mat4)> traverseNodeTree;
            traverseNodeTree = [&](aiNode* node, mat4 accTransform) {
                accTransform = accTransform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));
                for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                    auto mesh = scene->mMeshes[i];

                    meshSorted[mesh->mMaterialIndex].push_back({ mesh, accTransform });

                    indexCount += (mesh->mNumFaces * 3);
                    vertexCount += mesh->mNumVertices;
                    bonesCount += mesh->mNumBones;

                    hasTexCoords = mesh->mNumUVComponents[0] > 0 ? true : hasTexCoords;
                }

                for (uint32_t i = 0; i < node->mNumChildren; i++) {
                    traverseNodeTree(node->mChildren[i], accTransform);
                }
            };

            traverseNodeTree(scene->mRootNode, transform);

            hasTangents |= forceTangents;
            for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
                if (scene->mMaterials[i]->GetTextureCount(aiTextureType_NORMALS) > 0)
                    hasTangents = true;
                if (scene->mMaterials[i]->GetTextureCount(aiTextureType_HEIGHT) > 0)
                    hasTangents = true;
            }

            if (vertexCount > 65535) {
                meshData.indices.SetType(Mesh::ComponentFormat::UnsignedInt);
            }
            else {
                meshData.indices.SetType(Mesh::ComponentFormat::UnsignedShort);
            }

            meshData.vertices.SetType(Mesh::ComponentFormat::Float);
            meshData.normals.SetType(Mesh::ComponentFormat::PackedFloat);
            meshData.texCoords.SetType(Mesh::ComponentFormat::HalfFloat);
            meshData.tangents.SetType(Mesh::ComponentFormat::PackedFloat);

            meshData.SetIndexCount(indexCount);
            meshData.SetVertexCount(vertexCount);

            if (scene->HasAnimations() || bonesCount > 0) {



            }

            auto min = vec3(std::numeric_limits<float>::max());
            auto max = vec3(-std::numeric_limits<float>::max());

            uint32_t usedFaces = 0;
            uint32_t usedVertices = 0;
            uint32_t loadedVertices = 0;

            std::vector<uint32_t> indices(indexCount);

            std::vector<vec3> vertices(vertexCount);
            std::vector<vec2> texCoords(hasTexCoords ? vertexCount : 0);
            std::vector<vec4> normals(vertexCount);
            std::vector<vec4> tangents(hasTangents ? vertexCount : 0);

            auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;
            auto rgbSupport = graphicsDevice->CheckFormatSupport(VK_FORMAT_R8G8B8_UNORM,
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

            std::atomic_int32_t counter = 0;
            std::vector<MaterialImages> materialImages(scene->mNumMaterials);
            auto loadImagesLambda = [&]() {
                auto i = counter++;

                while (i < int32_t(materialImages.size())) {

                    auto& images = materialImages[i];

                    LoadMaterialImages(scene->mMaterials[i], images, directoryPath,
                        isObj, hasTangents, maxTextureResolution, rgbSupport);

                    i = counter++;
                }

            };

            auto threadCount = std::thread::hardware_concurrency();
            std::vector<std::thread> threads;
            for (int32_t i = 0; i < threadCount; i++) {
                threads.emplace_back(std::thread{ loadImagesLambda });
            }

            for (int32_t i = 0; i < threadCount; i++) {
                threads[i].join();
            }

            meshData.subData = std::vector<Mesh::MeshSubData>(scene->mNumMaterials);
            meshData.materials = std::vector<Material>(scene->mNumMaterials);

            for (uint32_t i = 0; i < scene->mNumMaterials; i++) {

                auto& material = meshData.materials[i];
                auto& images = materialImages[i];
                auto& subData = meshData.subData[i];

                // No material loading for now
                LoadMaterial(scene->mMaterials[i], images, material);

                subData.material = &material;
                subData.materialIdx = i;
                subData.indicesOffset = usedFaces * 3;

                for (auto assimpMesh : meshSorted[i]) {
                
                    auto mesh = assimpMesh.mesh;
                    auto matrix = assimpMesh.transform;

                    // Copy vertices
                    for (uint32_t j = 0; j < mesh->mNumVertices; j++) {

                        vec3 vertex = vec3(matrix * vec4(mesh->mVertices[j].x, 
                            mesh->mVertices[j].y, mesh->mVertices[j].z, 1.0f));

                        vertices[usedVertices] = vertex;

                        max = glm::max(vertex, max);
                        min = glm::min(vertex, min);

                        vec3 normal = vec3(matrix * vec4(mesh->mNormals[j].x, mesh->mNormals[j].y,
                            mesh->mNormals[j].z, 0.0f));
                        normal = normalize(normal);

                        normals[usedVertices] = vec4(normal, 0.0f);

                        if (hasTangents && mesh->mTangents != nullptr) {
                            vec3 tangent = vec3(matrix * vec4(mesh->mTangents[j].x, 
                                mesh->mTangents[j].y, mesh->mTangents[j].z, 0.0f));
                            tangent = normalize(tangent - normal * dot(normal, tangent));

                            vec3 estimatedBitangent = normalize(cross(tangent, normal));
                            vec3 correctBitangent = vec3(matrix * vec4(mesh->mBitangents[j].x,
                                mesh->mBitangents[j].y, mesh->mBitangents[j].z, 0.0f));
                            correctBitangent = normalize(correctBitangent);

                            float dotProduct = dot(estimatedBitangent, correctBitangent);

                            tangents[usedVertices] = vec4(tangent, dotProduct <= 0.0f ? -1.0f : 1.0f);
                        }

                        if (hasTexCoords && mesh->mTextureCoords[0] != nullptr) {
                            texCoords[usedVertices] = vec2(mesh->mTextureCoords[0][j].x,
                                                           mesh->mTextureCoords[0][j].y);
                        }

                        usedVertices++;

                    }
                    // Copy indices
                    for (uint32_t j = 0; j < mesh->mNumFaces; j++) {
                        for (uint32_t k = 0; k < 3; k++) {
                            indices[usedFaces * 3 + k] = mesh->mFaces[j].mIndices[k] + loadedVertices;
                        }
                        usedFaces++;
                    }

                    loadedVertices = usedVertices;

                }

                subData.indicesCount = usedFaces * 3 - subData.indicesOffset;

            }

            materialImages.clear();

            meshData.aabb = Volume::AABB(min, max);

            meshData.indices.Set(indices);
            meshData.vertices.Set(vertices);
            meshData.normals.Set(normals);

            if (hasTexCoords)
                meshData.texCoords.Set(texCoords);
            if (hasTangents)
                meshData.tangents.Set(tangents);

            meshData.filename = filename;

            return CreateRef(meshData);

        }

        void ModelLoader::LoadMaterial(aiMaterial* assimpMaterial, MaterialImages& images, Material& material) {

            bool roughnessMetalnessTexture = false;

            aiString name;

            aiColor3D diffuse;
            aiColor3D emissive;
            aiColor3D specular;
            float specularHardness = 0.0f;
            float specularIntensity = 0.0f;
            float metalness = 0.0f;
            bool twoSided = false;

            int32_t shadingModel;
            assimpMaterial->Get(AI_MATKEY_SHADING_MODEL, shadingModel);

            // It seems like these methods are not guaranteed to set the value
            assimpMaterial->Get(AI_MATKEY_NAME, name);
            assimpMaterial->Get(AI_MATKEY_SHININESS, specularHardness);
            assimpMaterial->Get(AI_MATKEY_OPACITY, material.opacity);
            assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
            assimpMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
            assimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specular);
            assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness);
            assimpMaterial->Get(AI_MATKEY_TWOSIDED, twoSided);

            material.name = std::string(name.C_Str());

            material.baseColor = vec3(diffuse.r, diffuse.g, diffuse.b);
            material.emissiveColor = vec3(emissive.r, emissive.g, emissive.b);

            material.displacementScale = 0.01f;
            material.normalScale = 0.5f;

            // Avoid NaN
            specularHardness = glm::max(1.0f, specularHardness);
            material.roughness = glm::clamp(powf(1.0f / (0.5f * specularHardness + 1.0f), 0.25f), 0.0f, 1.0f);
            material.ao = 1.0f;

            metalness = metalness == 0.0f ? glm::max(specular.r, glm::max(specular.g, specular.b)) : metalness;
            material.metalness = glm::clamp(metalness, 0.0f, 1.0f);

            material.twoSided = twoSided;
            
            if (images.baseColorImage && images.baseColorImage->HasData()) {
                material.baseColorMap = std::make_shared<Texture::Texture2D>(images.baseColorImage);
                material.baseColorMapPath = images.baseColorImage->fileName;
            }
            if (images.opacityImage && images.opacityImage->HasData()) {
                material.opacityMap = std::make_shared<Texture::Texture2D>(images.opacityImage);
                material.opacityMapPath = images.opacityImage->fileName;
            }
            if (images.roughnessImage && images.roughnessImage->HasData()) {
                material.roughnessMap = std::make_shared<Texture::Texture2D>(images.roughnessImage);
                material.roughnessMapPath = images.roughnessImage->fileName;
            }
            if (images.metallicImage && images.metallicImage->HasData()) {
                material.metalnessMap = std::make_shared<Texture::Texture2D>(images.metallicImage);
                material.metalnessMapPath = images.metallicImage->fileName;
            }
            if (images.normalImage && images.normalImage->HasData()) {
                material.normalMap = std::make_shared<Texture::Texture2D>(images.normalImage);
                material.normalMapPath = images.normalImage->fileName;
            }
            if (images.displacementImage && images.displacementImage->HasData()) {
                material.displacementMap = std::make_shared<Texture::Texture2D>(images.displacementImage);
                material.displacementMapPath = images.displacementImage->fileName;
            }
            
            // Probably foliage
            if (material.HasOpacityMap() ||
                material.opacity < 1.0f) {
                material.twoSided = true;
            }
            
        }

        void ModelLoader::LoadMaterialImages(aiMaterial* material, MaterialImages& images, const std::string& directory,
            bool isObj, bool hasTangents, int32_t maxTextureResolution, bool rgbSupport) {
            if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0 ||
                material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString aiPath;
                if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
                    material->GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath);
                else
                    material->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                images.baseColorImage = ImageLoader::LoadImage<uint8_t>(path, true, 0, maxTextureResolution);

                if (images.baseColorImage->channels == 1) {
                    auto imageData = images.baseColorImage->GetData();
                    std::vector<uint8_t> data(images.baseColorImage->width * images.baseColorImage->height * 3);
                    for (size_t i = 0; i < data.size(); i += 3) {
                        data[i + 0] = imageData[i / 3];
                        data[i + 1] = imageData[i / 3];
                        data[i + 2] = imageData[i / 3];
                    }
                    images.baseColorImage = CreateRef<Common::Image<uint8_t>>(images.baseColorImage->width,
                        images.baseColorImage->height, 3);
                    images.baseColorImage->SetData(data);
                }
                else if (images.baseColorImage->channels == 4) {
                    images.opacityImage = images.baseColorImage->GetChannelImage(3, 1);
                    images.baseColorImage = images.baseColorImage->GetChannelImage(0, 3);
                }

                // Some device e.g. Mac M1 don't support the necessary format
                if (!rgbSupport) {
                    images.baseColorImage->ExpandToChannelCount(4, 255);
                }
            }
            if (material->GetTextureCount(aiTextureType_OPACITY) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_OPACITY, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                images.opacityImage = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
            }
            if ((material->GetTextureCount(aiTextureType_NORMALS) > 0 ||
                (material->GetTextureCount(aiTextureType_HEIGHT) > 0 && isObj))
                && hasTangents) {
                aiString aiPath;
                if (isObj)
                    material->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                else
                    material->GetTexture(aiTextureType_NORMALS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                images.normalImage = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);
                // Might still be a traditional displacement map
                if (images.normalImage->channels == 1 && isObj) {
                    images.displacementImage = images.normalImage;
                    images.normalImage = ApplySobelFilter(images.normalImage);
                }
                if (images.normalImage->channels == 4) {
                    images.normalImage = images.normalImage->GetChannelImage(0, 3);
                }
                // Some device e.g. Mac M1 don't support the necessary format
                if (!rgbSupport) {
                    images.normalImage->ExpandToChannelCount(4, 255);
                }
            }
            if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                images.roughnessImage = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);
                if (images.roughnessImage->channels > 1) {
                    images.metallicImage = images.roughnessImage->GetChannelImage(2, 1);
                    images.roughnessImage = images.roughnessImage->GetChannelImage(1, 1);
                }

            }
            if (material->GetTextureCount(aiTextureType_METALNESS) > 0 &&
                images.metallicImage && !images.metallicImage->HasData()) {
                aiString aiPath;
                material->GetTexture(aiTextureType_METALNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                images.metallicImage = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
            }
            if (material->GetTextureCount(aiTextureType_SPECULAR) > 0 &&
                images.metallicImage && !images.metallicImage->HasData()) {
                aiString aiPath;
                material->GetTexture(aiTextureType_SPECULAR, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                images.metallicImage = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
            }
            if (material->GetTextureCount(aiTextureType_HEIGHT) > 0 && !isObj && hasTangents) {
                aiString aiPath;
                material->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                images.displacementImage = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
            }
        }

        std::string ModelLoader::GetDirectoryPath(std::string filename) {

            auto directoryPath = Common::Path::GetDirectory(filename);

            if (directoryPath.length())
                directoryPath += "/";

            return directoryPath;

        }

    }

}