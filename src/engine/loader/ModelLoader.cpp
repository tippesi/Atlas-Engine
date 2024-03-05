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

        Ref<Mesh::Mesh> ModelLoader::LoadMesh(const std::string& filename,
            bool forceTangents, int32_t maxTextureResolution) {

            return LoadMesh(filename, Mesh::MeshMobility::Stationary, forceTangents,
                maxTextureResolution);

        }

        Ref<Mesh::Mesh> ModelLoader::LoadMesh(const std::string& filename,
            Mesh::MeshMobility mobility, bool forceTangents,
            int32_t maxTextureResolution) {

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
                throw ResourceLoadException(filename, "Error processing model "
                    + std::string(importer.GetErrorString()));
            }

            int32_t indexCount = 0;
            int32_t vertexCount = 0;
            int32_t bonesCount = 0;

            bool hasTangents = false;
            bool hasTexCoords = false;
            bool hasVertexColors = false;

            struct AssimpMesh {
                aiMesh* mesh;
                mat4 transform;
            };

            std::vector<std::vector<AssimpMesh>> meshSorted(scene->mNumMaterials);

            std::function<void(aiNode*, mat4)> traverseNodeTree;
            traverseNodeTree = [&](aiNode* node, mat4 accTransform) {
                accTransform = accTransform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));
                for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                    auto meshId = node->mMeshes[i];
                    auto mesh = scene->mMeshes[meshId];

                    meshSorted[mesh->mMaterialIndex].push_back({ mesh, accTransform });

                    indexCount += (mesh->mNumFaces * 3);
                    vertexCount += mesh->mNumVertices;
                    bonesCount += mesh->mNumBones;

                    hasTexCoords |= mesh->mNumUVComponents[0] > 0;
                    hasVertexColors |= mesh->HasVertexColors(0);
                }

                for (uint32_t i = 0; i < node->mNumChildren; i++) {
                    traverseNodeTree(node->mChildren[i], accTransform);
                }
            };

            traverseNodeTree(scene->mRootNode, mat4(1.0f));

            hasTangents |= forceTangents;
            for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
                if (scene->mMaterials[i]->GetTextureCount(aiTextureType_NORMALS) > 0)
                    hasTangents = true;
                if (scene->mMaterials[i]->GetTextureCount(aiTextureType_HEIGHT) > 0)
                    hasTangents = true;
            }

            auto mesh = CreateRef<Mesh::Mesh>();
            mesh->mobility = mobility;
            auto& meshData = mesh->data;

            if (vertexCount > 65535) {
                meshData.indices.SetType(Mesh::ComponentFormat::UnsignedInt);
            }
            else {
                meshData.indices.SetType(Mesh::ComponentFormat::UnsignedShort);
            }

            meshData.vertices.SetType(Mesh::ComponentFormat::Float);
            meshData.normals.SetType(Mesh::ComponentFormat::PackedNormal);
            meshData.texCoords.SetType(Mesh::ComponentFormat::HalfFloat);
            meshData.tangents.SetType(Mesh::ComponentFormat::PackedNormal);
            meshData.colors.SetType(Mesh::ComponentFormat::PackedColor);

            meshData.SetIndexCount(indexCount);
            meshData.SetVertexCount(vertexCount);

            if (scene->HasAnimations() || bonesCount > 0) {



            }

            auto min = vec3(std::numeric_limits<float>::max());
            auto max = vec3(-std::numeric_limits<float>::max());

            uint32_t usedFaces = 0;
            uint32_t usedVertices = 0;
            uint32_t loadedVertices = 0;

            auto& indices = meshData.indices;

            auto& vertices = meshData.vertices;
            auto& texCoords = meshData.texCoords;
            auto& normals = meshData.normals;
            auto& tangents = meshData.tangents;
            auto& colors = meshData.colors;

            indices.SetElementCount(indexCount);

            vertices.SetElementCount(vertexCount);
            texCoords.SetElementCount(hasTexCoords ? vertexCount : 0);
            normals.SetElementCount(vertexCount);
            tangents.SetElementCount(hasTangents ? vertexCount : 0);
            colors.SetElementCount(hasVertexColors ? vertexCount : 0);

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
            for (uint32_t i = 0; i < threadCount; i++) {
                threads.emplace_back(std::thread{ loadImagesLambda });
            }

            for (uint32_t i = 0; i < threadCount; i++) {
                threads[i].join();
            }

            for (auto& images : materialImages) {
                ImagesToTexture(images);
            }

            meshData.subData = std::vector<Mesh::MeshSubData>(scene->mNumMaterials);

            float radius = 0.0f;
            for (uint32_t i = 0; i < scene->mNumMaterials; i++) {

                auto material = CreateRef<Material>();
                meshData.materials.push_back(material);

                auto& images = materialImages[i];
                auto& subData = meshData.subData[i];

                LoadMaterial(scene->mMaterials[i], images, *material, directoryPath, isObj, hasTangents);

                material->vertexColors = hasVertexColors;

                subData.material = material;
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

                        radius = glm::max(radius, glm::length(vertex));
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

                        if (hasVertexColors && mesh->mColors[0] != nullptr) {
                            colors[j] = vec4(mesh->mColors[0][j].r, mesh->mColors[0][j].g,
                                mesh->mColors[0][j].b, mesh->mColors[0][j].a);
                        }
                        else if (hasVertexColors && mesh->mColors[0] == nullptr) {
                            colors[j] = vec4(1.0f);
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

            importer.FreeScene();

            materialImages.clear();

            meshData.aabb = Volume::AABB(min, max);
            meshData.radius = radius;

            meshData.filename = filename;

            mesh->name = meshData.filename;
            mesh->UpdateData();

            return mesh;

        }

        Ref<Scene::Scene> ModelLoader::LoadScene(const std::string& filename, vec3 min, vec3 max,
            int32_t depth, bool forceTangents, int32_t maxTextureResolution) {

            auto directoryPath = GetDirectoryPath(filename);

            AssetLoader::UnpackFile(filename);

            auto fileType = Common::Path::GetFileType(filename);
            bool isObj = fileType == "obj" || fileType == "OBJ";

            Assimp::Importer importer;
            importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);

            // Use aiProcess_GenSmoothNormals in case model lacks normals and smooth normals are needed
            // Use aiProcess_GenNormals in case model lacks normals and flat normals are needed
            // Right now we just use flat normals everytime normals are missing.
            const aiScene* assimpScene = importer.ReadFile(AssetLoader::GetFullPath(filename),
                aiProcess_CalcTangentSpace |
                aiProcess_JoinIdenticalVertices |
                aiProcess_Triangulate |
                aiProcess_OptimizeGraph |
                aiProcess_OptimizeMeshes |
                aiProcess_RemoveRedundantMaterials |
                aiProcess_GenNormals |
                aiProcess_LimitBoneWeights |
                aiProcess_ImproveCacheLocality);

            if (!assimpScene) {
                throw ResourceLoadException(filename, "Error processing model "
                    + std::string(importer.GetErrorString()));
            }

            std::atomic_int32_t counter = 0;
            MaterialImages materialImages;
            auto loadImagesLambda = [&]() {
                auto i = counter++;

                while (i < int32_t(assimpScene->mNumMaterials)) {

                    LoadMaterialImages(assimpScene->mMaterials[i], materialImages, directoryPath,
                        isObj, true, maxTextureResolution, false);

                    i = counter++;
                }

            };

            auto threadCount = std::thread::hardware_concurrency();
            std::vector<std::thread> threads;
            for (uint32_t i = 0; i < threadCount; i++) {
                threads.emplace_back(std::thread{ loadImagesLambda });
            }

            for (uint32_t i = 0; i < threadCount; i++) {
                threads[i].join();
            }

            ImagesToTexture(materialImages);

            std::map<aiMesh*, ResourceHandle<Mesh::Mesh>> meshMap;

            for (uint32_t i = 0; i < assimpScene->mNumMeshes; i++) {
                auto assimpMesh = assimpScene->mMeshes[i];
                auto materialIdx = assimpMesh->mMaterialIndex;
                auto assimpMaterial = assimpScene->mMaterials[materialIdx];

                uint32_t indexCount = assimpMesh->mNumFaces * 3;
                uint32_t vertexCount = assimpMesh->mNumVertices;

                bool hasTangents = false;
                bool hasVertexColors = false;
                bool hasTexCoords = assimpMesh->mNumUVComponents[0] > 0;

                auto mesh = CreateRef<Mesh::Mesh>();
                auto& meshData = mesh->data;

                hasTangents |= forceTangents;
                if (assimpMaterial->GetTextureCount(aiTextureType_NORMALS) > 0)
                    hasTangents = true;
                if (assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0)
                    hasTangents = true;

                if (vertexCount > 65535) {
                    meshData.indices.SetType(Mesh::ComponentFormat::UnsignedInt);
                }
                else {
                    meshData.indices.SetType(Mesh::ComponentFormat::UnsignedShort);
                }

                hasVertexColors = assimpMesh->HasVertexColors(0);

                meshData.SetIndexCount(indexCount);
                meshData.SetVertexCount(vertexCount);

                auto& indices = meshData.indices;

                auto& vertices = meshData.vertices;
                auto& texCoords = meshData.texCoords;
                auto& normals = meshData.normals;
                auto& tangents = meshData.tangents;
                auto& colors = meshData.colors;

                indices.SetElementCount(indexCount);

                vertices.SetElementCount(vertexCount);
                texCoords.SetElementCount(hasTexCoords ? vertexCount : 0);
                normals.SetElementCount(vertexCount);
                tangents.SetElementCount(hasTangents ? vertexCount : 0);
                colors.SetElementCount(hasVertexColors ? vertexCount : 0);

                auto material = CreateRef<Material>();
                meshData.materials.push_back(material);

                auto min = vec3(std::numeric_limits<float>::max());
                auto max = vec3(-std::numeric_limits<float>::max());

                LoadMaterial(assimpMaterial, materialImages, *material, directoryPath, isObj, hasTangents);

                material->vertexColors = hasVertexColors;

                float radius = 0.0f;
                for (uint32_t j = 0; j < assimpMesh->mNumVertices; j++) {

                    vec3 vertex = vec3(assimpMesh->mVertices[j].x,
                        assimpMesh->mVertices[j].y, assimpMesh->mVertices[j].z);

                    vertices[j] = vertex;

                    radius = glm::max(radius, glm::length(vertex));
                    max = glm::max(vertex, max);
                    min = glm::min(vertex, min);

                    vec3 normal = vec3(assimpMesh->mNormals[j].x,
                        assimpMesh->mNormals[j].y, assimpMesh->mNormals[j].z);
                    normal = normalize(normal);

                    normals[j] = vec4(normal, 0.0f);

                    if (hasTangents && assimpMesh->mTangents != nullptr) {
                        vec3 tangent = vec3(assimpMesh->mTangents[j].x,
                            assimpMesh->mTangents[j].y, assimpMesh->mTangents[j].z);
                        tangent = normalize(tangent - normal * dot(normal, tangent));

                        vec3 estimatedBitangent = normalize(cross(tangent, normal));
                        vec3 correctBitangent = vec3(assimpMesh->mBitangents[j].x,
                            assimpMesh->mBitangents[j].y, assimpMesh->mBitangents[j].z);
                        correctBitangent = normalize(correctBitangent);

                        float dotProduct = dot(estimatedBitangent, correctBitangent);

                        tangents[j] = vec4(tangent, dotProduct <= 0.0f ? -1.0f : 1.0f);
                    }

                    if (hasTexCoords && assimpMesh->mTextureCoords[0] != nullptr) {
                        texCoords[j] = vec2(assimpMesh->mTextureCoords[0][j].x,
                            assimpMesh->mTextureCoords[0][j].y);
                    }

                    if (hasVertexColors && assimpMesh->mColors[0] != nullptr) {
                        colors[j] = vec4(assimpMesh->mColors[0][j].r, assimpMesh->mColors[0][j].g,
                            assimpMesh->mColors[0][j].b, assimpMesh->mColors[0][j].a);
                    }

                }

                // Copy indices
                for (uint32_t j = 0; j < assimpMesh->mNumFaces; j++) {
                    for (uint32_t k = 0; k < 3; k++) {
                        indices[j * 3 + k] = assimpMesh->mFaces[j].mIndices[k];
                    }
                }

                meshData.aabb = Volume::AABB(min, max);
                meshData.radius = radius;

                meshData.subData.push_back({
                    .indicesOffset = 0,
                    .indicesCount = indexCount,

                    .material = material,
                    .materialIdx = 0,

                    .aabb = meshData.aabb
                });

                meshData.filename = std::string(assimpMesh->mName.C_Str());
                mesh->name = meshData.filename;
                mesh->UpdateData();

                auto handle = ResourceManager<Mesh::Mesh>::AddResource(filename + "_" + mesh->name, mesh);
                meshMap[assimpMesh] = handle;
            }

            auto scene = CreateRef<Scene::Scene>(filename, min, max, depth);

            auto rootEntity = scene->CreateEntity();
            rootEntity.AddComponent<NameComponent>("Root");
            auto& rootHierarchy = rootEntity.AddComponent<HierarchyComponent>();
            rootHierarchy.root = true;

            int32_t triangleCount = 0;

            std::function<void(aiNode*, Scene::Entity)> traverseNodeTree;
            traverseNodeTree = [&](aiNode* node, Scene::Entity parentEntity) {
                auto nodeTransform = glm::transpose(glm::make_mat4(&node->mTransformation.a1));

                for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                    auto meshId = node->mMeshes[i];
                    auto mesh = assimpScene->mMeshes[meshId];

                    triangleCount += mesh->mNumFaces;

                    auto entity = scene->CreatePrefab<Scene::Prefabs::MeshInstance>(meshMap[mesh], nodeTransform);
                    parentEntity.GetComponent<HierarchyComponent>().AddChild(entity);
                }

                for (uint32_t i = 0; i < node->mNumChildren; i++) {
                    auto nodeEntity = scene->CreatePrefab<Scene::Prefabs::Node>(node->mChildren[i]->mName.C_Str(), nodeTransform);
                    auto& hierarchy = nodeEntity.GetComponent<HierarchyComponent>();

                    parentEntity.GetComponent<HierarchyComponent>().AddChild(nodeEntity);

                    traverseNodeTree(node->mChildren[i], nodeEntity);
                }
            };

            traverseNodeTree(assimpScene->mRootNode, rootEntity);

            return scene;

        }

        void ModelLoader::LoadMaterial(aiMaterial* assimpMaterial, MaterialImages& images, Material& material, 
            const std::string& directory, bool isObj, bool hasTangents) {

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

            material.emissiveIntensity = glm::max(glm::max(emissive.r, glm::max(emissive.g, emissive.b)), 1.0f);
            material.emissiveColor = vec3(emissive.r, emissive.g, emissive.b) / material.emissiveIntensity;

            material.displacementScale = 0.01f;
            material.normalScale = 0.5f;

            // Avoid NaN
            specularHardness = glm::max(1.0f, specularHardness);
            material.roughness = glm::clamp(powf(1.0f / (0.5f * specularHardness + 1.0f), 0.25f), 0.0f, 1.0f);
            material.ao = 1.0f;

            metalness = metalness == 0.0f ? glm::max(specular.r, glm::max(specular.g, specular.b)) : metalness;
            material.metalness = glm::clamp(metalness, 0.0f, 1.0f);

            material.twoSided = twoSided;

            if (shadingModel == aiShadingMode_PBR_BRDF) {
                assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, material.metalness);
                assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, material.roughness);
            }

            if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0 ||
                assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString aiPath;
                if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
                    assimpMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath);
                else
                    assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.baseColorTextures.contains(path)) {
                    material.baseColorMap = images.baseColorTextures[path];
                    material.baseColorMapPath = images.baseColorImages[path]->fileName;
                }
                if (images.opacityTextures.contains(path)) {
                    material.opacityMap = images.opacityTextures[path];
                    material.opacityMapPath = images.opacityImages[path]->fileName;
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_OPACITY) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_OPACITY, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.opacityTextures.contains(path)) {
                    material.opacityMap = images.opacityTextures[path];
                    material.opacityMapPath = images.opacityImages[path]->fileName;
                }
            }
            if ((assimpMaterial->GetTextureCount(aiTextureType_NORMALS) > 0 ||
                (assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0 && isObj))
                && hasTangents) {
                aiString aiPath;
                if (isObj)
                    assimpMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                else
                    assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.normalTextures.contains(path)) {
                    material.normalMap = images.normalTextures[path];
                    material.normalMapPath = images.normalImages[path]->fileName;
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.roughnessTextures.contains(path)) {
                    material.roughnessMap = images.roughnessTextures[path];
                    material.roughnessMapPath = images.roughnessImages[path]->fileName;
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_METALNESS) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_METALNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.metallicTextures.contains(path)) {
                    material.metalnessMap = images.metallicTextures[path];
                    material.metalnessMapPath = images.metallicImages[path]->fileName;
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_SPECULAR, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.metallicTextures.contains(path) && !material.HasMetalnessMap()) {
                    material.metalnessMap = images.metallicTextures[path];
                    material.metalnessMapPath = images.metallicImages[path]->fileName;
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0 && !isObj && hasTangents) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.displacementTextures.contains(path)) {
                    material.displacementMap = images.displacementTextures[path];
                    material.displacementMapPath = images.displacementImages[path]->fileName;
                }
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

                if (!images.baseColorImages.contains(path)) {

                    Ref<Common::Image<uint8_t>> opacityImage = nullptr;
                    auto image = ImageLoader::LoadImage<uint8_t>(path, true, 0, maxTextureResolution);

                    if (image->channels == 1) {
                        auto imageData = image->GetData();
                        std::vector<uint8_t> data(image->width * image->height * 3);
                        for (size_t i = 0; i < data.size(); i += 3) {
                            data[i + 0] = imageData[i / 3];
                            data[i + 1] = imageData[i / 3];
                            data[i + 2] = imageData[i / 3];
                        }
                        image = CreateRef<Common::Image<uint8_t>>(image->width, image->height, 3);
                        image->SetData(data);
                    }
                    else if (image->channels == 4) {
                        opacityImage = image->GetChannelImage(3, 1);
                        image = image->GetChannelImage(0, 3);
                    }

                    // Some device e.g. Mac M1 don't support the necessary format
                    if (!rgbSupport) {
                        image->ExpandToChannelCount(4, 255);
                    }

                    images.baseColorImages[path] = image;
                    if (!images.opacityImages.contains(path) && opacityImage != nullptr)
                        images.opacityImages[path] = opacityImage;
                }
            }
            if (material->GetTextureCount(aiTextureType_OPACITY) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_OPACITY, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.opacityImages.contains(path)) {
                    images.opacityImages[path] = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
                }
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
                if (!images.normalImages.contains(path)) {
                    Ref<Common::Image<uint8_t>> displacementImage = nullptr;
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);
                    // Might still be a traditional displacement map
                    if (image->channels == 1 && isObj) {
                        displacementImage = image;
                        image = ApplySobelFilter(image);
                    }
                    if (image->channels == 4) {
                        image = image->GetChannelImage(0, 3);
                    }

                    // Some device e.g. Mac M1 don't support the necessary format
                    if (!rgbSupport) {
                        image->ExpandToChannelCount(4, 255);
                    }

                    images.normalImages[path] = image;
                    if (!images.displacementImages.contains(path) && displacementImage != nullptr)
                        images.displacementImages[path] = displacementImage;
                }
            }
            if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));

                if (!images.roughnessImages.contains(path)) {
                    Ref<Common::Image<uint8_t>> metallicImage = nullptr;
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);

                    // It's common in GLTF that these are stored in one texture
                    if (image->channels > 1) {
                        metallicImage = image->GetChannelImage(2, 1);
                        image = image->GetChannelImage(1, 1);
                    }

                    images.roughnessImages[path] = image;
                    if (!images.metallicImages.contains(path) && metallicImage != nullptr)
                        images.metallicImages[path] = metallicImage;
                }
            }
            if (material->GetTextureCount(aiTextureType_METALNESS) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_METALNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.metallicImages.contains(path))
                    images.metallicImages[path] = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
            }
            if (material->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_SPECULAR, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.metallicImages.contains(path))
                    images.metallicImages[path] = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
            }
            if (material->GetTextureCount(aiTextureType_HEIGHT) > 0 && !isObj && hasTangents) {
                aiString aiPath;
                material->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.displacementImages.contains(path))
                    images.displacementImages[path] = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
            }
        }

        void ModelLoader::ImagesToTexture(MaterialImages& images) {

            for (const auto& [path, image] : images.baseColorImages) {
                images.baseColorTextures[path] = std::make_shared<Texture::Texture2D>(image);
            }
            for (const auto& [path, image] : images.opacityImages) {
                images.opacityTextures[path] = std::make_shared<Texture::Texture2D>(image);
            }
            for (const auto& [path, image] : images.normalImages) {
                images.normalTextures[path] = std::make_shared<Texture::Texture2D>(image);
            }
            for (const auto& [path, image] : images.roughnessImages) {
                images.roughnessTextures[path] = std::make_shared<Texture::Texture2D>(image);
            }
            for (const auto& [path, image] : images.metallicImages) {
                images.metallicTextures[path] = std::make_shared<Texture::Texture2D>(image);
            }
            for (const auto& [path, image] : images.displacementImages) {
                images.displacementTextures[path] = std::make_shared<Texture::Texture2D>(image);
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