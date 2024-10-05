#include "ModelImporter.h"
#include "ImageLoader.h"
#include "MaterialLoader.h"
#include "MeshLoader.h"
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

        Ref<Mesh::Mesh> ModelImporter::ImportMesh(const std::string& filename,
            bool saveToDisk, int32_t maxTextureResolution) {

            return ImportMesh(filename, Mesh::MeshMobility::Stationary, saveToDisk,
                maxTextureResolution);

        }

        Ref<Mesh::Mesh> ModelImporter::ImportMesh(const std::string& filename,
            Mesh::MeshMobility mobility, bool saveToDisk,
            int32_t maxTextureResolution) {

            ImporterState state;
            InitImporterState(state, filename, true);

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

            std::vector<std::vector<AssimpMesh>> meshSorted(state.scene->mNumMaterials);

            std::function<void(aiNode*, mat4)> traverseNodeTree;
            traverseNodeTree = [&](aiNode* node, mat4 accTransform) {
                accTransform = accTransform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));
                for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                    auto meshId = node->mMeshes[i];
                    auto mesh = state.scene->mMeshes[meshId];

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

            traverseNodeTree(state.scene->mRootNode, mat4(1.0f));

            for (uint32_t i = 0; i < state.scene->mNumMaterials; i++) {
                if (state.scene->mMaterials[i]->GetTextureCount(aiTextureType_NORMALS) > 0)
                    hasTangents = true;
                if (state.scene->mMaterials[i]->GetTextureCount(aiTextureType_HEIGHT) > 0)
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

            if (state.scene->HasAnimations() || bonesCount > 0) {



            }

            auto radius = 0.0f;
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

            auto materials = ImportMaterials(state, maxTextureResolution, saveToDisk);

            meshData.subData = std::vector<Mesh::MeshSubData>(state.scene->mNumMaterials);

            for (uint32_t i = 0; i < state.scene->mNumMaterials; i++) {

                auto material = materials[i];
                meshData.materials.push_back(material);

                auto& subData = meshData.subData[i];

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

                        max = glm::max(vertex, max);
                        min = glm::min(vertex, min);
                        radius = glm::dot(vertex, vertex);

                        vec3 normal = vec3(matrix * vec4(mesh->mNormals[j].x, mesh->mNormals[j].y,
                            mesh->mNormals[j].z, 0.0f));
                        normal = normalize(normal);

                        normals[usedVertices] = vec4(normal, 0.0f);

                        if (hasTangents && mesh->mTangents != nullptr) {
                            // Avoid tangents that are fully zero
                            vec3 tangent = vec3(mesh->mTangents[j].x,
                                mesh->mTangents[j].y, mesh->mTangents[j].z) + 1e-9f;
                            tangent = vec3(matrix * vec4(tangent, 0.0f));
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

            state.importer.FreeScene();

            meshData.aabb = Volume::AABB(min, max);
            meshData.radius = glm::sqrt(radius);
            

            meshData.name = Common::Path::GetFileNameWithoutExtension(filename);

            mesh->name = meshData.name;
            mesh->UpdateData();

            auto meshFilename = state.paths.meshPath + mesh->name + ".aemesh";
            if (saveToDisk) {
                Log::Message("Imported mesh " + meshFilename);
                Loader::MeshLoader::SaveMesh(mesh, meshFilename, true);
            }

            return mesh;

        }

        Ref<Scene::Scene> ModelImporter::ImportScene(const std::string& filename, vec3 min, vec3 max,
            int32_t depth, bool saveToDisk, bool makeMeshesStatic,
            bool invertUVs, int32_t maxTextureResolution) {

            ImporterState state;
            InitImporterState(state, filename, false);

            auto materials = ImportMaterials(state, maxTextureResolution, saveToDisk);

            struct MeshInfo {
                ResourceHandle<Mesh::Mesh> mesh;
                vec3 offset;
            };

            std::vector<MeshInfo> meshes;
            meshes.resize(state.scene->mNumMeshes);
            
            std::mutex vertexColorMutex;

            JobGroup group;
            JobSystem::ExecuteMultiple(group, int32_t(meshes.size()), [&](const JobData& data) {
                auto i = data.idx;

                auto assimpMesh = state.scene->mMeshes[i];
                auto materialIdx = assimpMesh->mMaterialIndex;
                auto material = materials[materialIdx];

                uint32_t indexCount = assimpMesh->mNumFaces * 3;
                uint32_t vertexCount = assimpMesh->mNumVertices;

                bool hasTangents = false;
                bool hasVertexColors = false;
                bool hasTexCoords = assimpMesh->mNumUVComponents[0] > 0;

                auto mesh = CreateRef<Mesh::Mesh>();
                auto& meshData = mesh->data;

                if (material->HasNormalMap())
                    hasTangents = true;
                if (material->HasDisplacementMap())
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

                {
                    std::scoped_lock lock(vertexColorMutex);
                    material->vertexColors &= hasVertexColors;
                }
                meshData.materials.push_back(material);

                auto radius = 0.0f;
                auto min = vec3(std::numeric_limits<float>::max());
                auto max = vec3(-std::numeric_limits<float>::max());

                uint32_t uvChannel = material->uvChannel;
                for (uint32_t j = 0; j < assimpMesh->mNumVertices; j++) {

                    vec3 vertex = vec3(assimpMesh->mVertices[j].x,
                        assimpMesh->mVertices[j].y, assimpMesh->mVertices[j].z);

                    vertices[j] = vertex;

                    max = glm::max(vertex, max);
                    min = glm::min(vertex, min);
                    radius = glm::dot(vertex, vertex);

                    vec3 normal = vec3(assimpMesh->mNormals[j].x,
                        assimpMesh->mNormals[j].y, assimpMesh->mNormals[j].z);
                    normal = normalize(normal);

                    normals[j] = vec4(normal, 0.0f);

                    if (hasTangents && assimpMesh->mTangents != nullptr) {
                        // Avoid tangents that are fully zero
                        vec3 tangent = vec3(assimpMesh->mTangents[j].x,
                            assimpMesh->mTangents[j].y, assimpMesh->mTangents[j].z) + 1e-9f;
                        tangent = normalize(tangent - normal * dot(normal, tangent));

                        vec3 estimatedBitangent = normalize(cross(tangent, normal));
                        vec3 correctBitangent = vec3(assimpMesh->mBitangents[j].x,
                            assimpMesh->mBitangents[j].y, assimpMesh->mBitangents[j].z);
                        correctBitangent = normalize(correctBitangent);

                        float dotProduct = dot(estimatedBitangent, correctBitangent);
                        tangents[j] = vec4(tangent, dotProduct <= 0.0f ? -1.0f : 1.0f);
                    }

                    if (hasTexCoords && assimpMesh->mTextureCoords[uvChannel] != nullptr) {
                        texCoords[j] = vec2(assimpMesh->mTextureCoords[uvChannel][j].x,
                            assimpMesh->mTextureCoords[uvChannel][j].y);
                    }

                    if (hasVertexColors && assimpMesh->mColors[0] != nullptr) {
                        colors[j] = vec4(assimpMesh->mColors[0][j].r, assimpMesh->mColors[0][j].g,
                            assimpMesh->mColors[0][j].b, assimpMesh->mColors[0][j].a);
                    }

                }

                auto offset = (max + min) * 0.5f;
                // Recenter mesh
                min -= offset;
                max -= offset;
                for (uint32_t j = 0; j < assimpMesh->mNumVertices; j++) {
                    vertices[j] = vertices[j] - offset;
                }

                // Copy indices
                for (uint32_t j = 0; j < assimpMesh->mNumFaces; j++) {
                    for (uint32_t k = 0; k < 3; k++) {
                        indices[j * 3 + k] = assimpMesh->mFaces[j].mIndices[k];
                    }
                }

                meshData.aabb = Volume::AABB(min, max);
                meshData.radius = glm::sqrt(radius);

                meshData.subData.push_back({
                    .indicesOffset = 0,
                    .indicesCount = indexCount,

                    .material = material,
                    .materialIdx = 0,

                    .aabb = meshData.aabb
                    });

                meshData.name = std::string(assimpMesh->mName.C_Str());
                mesh->name = meshData.name;
                mesh->invertUVs = invertUVs;
                mesh->UpdateData();

                auto meshFilename = state.paths.meshPath + mesh->name + "_" + std::to_string(i) + ".aemesh";
                auto handle = ResourceManager<Mesh::Mesh>::AddResource(meshFilename, mesh);
                meshes[i] = { handle, offset };

                if (saveToDisk) {
                    Log::Message("Imported mesh " + meshFilename);
                    Loader::MeshLoader::SaveMesh(mesh, meshFilename, true);
                }
                });
            JobSystem::Wait(group);

            auto scene = CreateRef<Scene::Scene>(filename, min, max, depth);

            std::map<std::string, aiLight*> lightMap;
            for (uint32_t i = 0; i < state.scene->mNumLights; i++) {
                auto light = state.scene->mLights[i];
                lightMap[light->mName.C_Str()] = light;
            }

            auto rootEntity = scene->CreateEntity();
            rootEntity.AddComponent<NameComponent>("Root");
            auto& rootHierarchy = rootEntity.AddComponent<HierarchyComponent>();
            rootHierarchy.root = true;

            std::function<void(aiNode*, Scene::Entity)> traverseNodeTree;
            traverseNodeTree = [&](aiNode* node, Scene::Entity parentEntity) {
                auto nodeTransform = glm::transpose(glm::make_mat4(&node->mTransformation.a1));

                auto& parentNameComp = parentEntity.GetComponent<NameComponent>();

                for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                    auto meshId = node->mMeshes[i];
                    auto& meshInfo = meshes[meshId];

                    auto instanceTransform = glm::translate(meshInfo.offset);

                    auto entity = scene->CreatePrefab<Scene::Prefabs::MeshInstance>(
                        meshInfo.mesh, nodeTransform * instanceTransform, makeMeshesStatic);

                    auto name = std::string(meshInfo.mesh->name) + "_" + std::to_string(entity);
                    entity.AddComponent<NameComponent>(name);
                    
                    parentEntity.GetComponent<HierarchyComponent>().AddChild(entity);
                }

                if (lightMap.contains(node->mName.C_Str()) && node->mNumChildren == 0) {
                    auto light = lightMap[node->mName.C_Str()];

                    auto lightType = LightType::DirectionalLight;
                    switch (light->mType) {
                    case aiLightSource_POINT: lightType = LightType::PointLight; break;
                    case aiLightSource_SPOT: lightType = LightType::SpotLight; break;
                    default: lightType = LightType::PointLight; break;
                    }

                    auto& lightComp = parentEntity.AddComponent<LightComponent>(lightType, LightMobility::StationaryLight);
                    lightComp.color = vec3(light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b);
                    lightComp.intensity = std::max(lightComp.color.r, std::max(lightComp.color.g, lightComp.color.b));
                    lightComp.color /= std::max(lightComp.intensity, 1e-9f);
                    lightComp.color = Common::ColorConverter::ConvertLinearToSRGB(lightComp.color);

                    float intensityRadius = glm::sqrt(lightComp.intensity / 0.15f);

                    if (lightType == LightType::PointLight) {                       
                        lightComp.properties.point.position = vec3(light->mPosition.x, light->mPosition.y, light->mPosition.z);
                        lightComp.properties.point.radius = std::max(glm::sqrt(100.0f * light->mAttenuationQuadratic), intensityRadius);
                        lightComp.AddPointShadow(3.0f, 1024);
                    }
                    if (lightType == LightType::SpotLight) {
                        lightComp.properties.spot.position = vec3(light->mPosition.x, light->mPosition.y, light->mPosition.z);
                        lightComp.properties.spot.direction = vec3(light->mDirection.x, light->mDirection.y, light->mDirection.z);
                        lightComp.properties.spot.innerConeAngle = light->mAngleInnerCone;
                        lightComp.properties.spot.outerConeAngle = light->mAngleOuterCone;
                        lightComp.properties.point.radius = std::max(glm::sqrt(100.0f * light->mAttenuationQuadratic), intensityRadius);
                        lightComp.AddSpotShadow(3.0f, 1024);
                    }
                }                

                for (uint32_t i = 0; i < node->mNumChildren; i++) {
                    auto nodeEntity = scene->CreatePrefab<Scene::Prefabs::Node>(node->mChildren[i]->mName.C_Str(), nodeTransform);
                    auto const& hierarchy = nodeEntity.GetComponent<HierarchyComponent>();

                    parentEntity.GetComponent<HierarchyComponent>().AddChild(nodeEntity);

                    traverseNodeTree(node->mChildren[i], nodeEntity);
                }
            };

            traverseNodeTree(state.scene->mRootNode, rootEntity);

            state.importer.FreeScene();

            return scene;

        }

        void ModelImporter::InitImporterState(ImporterState& state, const std::string& filename, bool optimizeMeshes) {

            state.paths = GetPaths(filename);

            AssetLoader::UnpackFile(filename);

            auto fileType = Common::Path::GetFileType(filename);
            state.isObj = fileType == "obj" || fileType == "OBJ";

            state.importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);

            uint32_t flags = aiProcess_CalcTangentSpace |
                aiProcess_JoinIdenticalVertices |
                aiProcess_Triangulate |
                aiProcess_OptimizeGraph |
                aiProcess_RemoveRedundantMaterials |
                aiProcess_GenNormals |
                aiProcess_LimitBoneWeights |
                aiProcess_ImproveCacheLocality;

            if (optimizeMeshes)
                flags |= aiProcess_OptimizeMeshes;

            // Use aiProcess_GenSmoothNormals in case model lacks normals and smooth normals are needed
            // Use aiProcess_GenNormals in case model lacks normals and flat normals are needed
            // Right now we just use flat normals everytime normals are missing.
            state.scene = state.importer.ReadFile(AssetLoader::GetFullPath(filename), flags);

            if (!state.scene) {
                throw ResourceLoadException(filename, "Error processing model "
                    + std::string(state.importer.GetErrorString()));
            }

        }

        std::vector<ResourceHandle<Material>> ModelImporter::ImportMaterials(ImporterState& state, int32_t maxTextureResolution, bool saveToDisk) {

            JobGroup group;
            JobSystem::ExecuteMultiple(group, state.scene->mNumMaterials, [&](const JobData& data) {
                LoadMaterialImages(state, state.scene->mMaterials[data.idx], true, maxTextureResolution);
                });
            JobSystem::Wait(group);

            auto imagesToSave = ImagesToTextures(state);

            if (saveToDisk) {
                JobSystem::ExecuteMultiple(group, int32_t(imagesToSave.size()), [&](const JobData& data) {
                    ImageLoader::SaveImage(imagesToSave[data.idx], imagesToSave[data.idx]->fileName);
                    });
                JobSystem::Wait(group);
            }

            std::vector<ResourceHandle<Material>> materials;
            for (uint32_t i = 0; i < state.scene->mNumMaterials; i++) {
                auto assimpMaterial = state.scene->mMaterials[i];
                auto material = CreateRef<Material>();

                LoadMaterial(state, assimpMaterial, *material);
                material->vertexColors = true;

                const auto materialFilename = state.paths.materialPath + material->name + ".aematerial";

                bool existed = false;
                auto handle = ResourceManager<Material>::AddResource(materialFilename, material, existed);
                if (existed)
                    Log::Warning("Material " + materialFilename + " was already loaded in the resource manager");
                materials.push_back(handle);

                if (saveToDisk)
                    Loader::MaterialLoader::SaveMaterial(material, materialFilename);
            }

            return materials;

        }

        void ModelImporter::LoadMaterial(ImporterState& state, aiMaterial* assimpMaterial, Material& material) {

            auto& images = state.images;
            auto& directory = state.paths.directoryPath;

            uint32_t uvChannel = 0;
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
            material.roughness = powf(1.0f / (0.5f * specularHardness + 1.0f), 0.25f);
            material.ao = 1.0f;

            metalness = metalness == 0.0f ? glm::max(specular.r, glm::max(specular.g, specular.b)) : metalness;
            material.metalness = metalness;

            material.twoSided = twoSided;

            if (shadingModel == aiShadingMode_PBR_BRDF) {
                assimpMaterial->Get(AI_MATKEY_METALLIC_FACTOR, material.metalness);
                assimpMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, material.roughness);
            }

            material.roughness = glm::clamp(material.roughness, 0.0f, 1.0f);
            material.metalness = glm::clamp(material.metalness, 0.0f, 1.0f);

            if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0 ||
                assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString aiPath;
                aiTextureMapping aiMapping;
                if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
                    assimpMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath, &aiMapping, &uvChannel);
                else
                    assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath, &aiMapping, &uvChannel);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.baseColorTextures.contains(path)) {
                    material.baseColorMap = images.baseColorTextures[path];
                }
                if (images.opacityTextures.contains(path)) {
                    material.opacityMap = images.opacityTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_OPACITY) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_OPACITY, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.opacityTextures.contains(path)) {
                    material.opacityMap = images.opacityTextures[path];
                }
            }
            if ((assimpMaterial->GetTextureCount(aiTextureType_NORMALS) > 0 ||
                (assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0 && state.isObj))) {
                aiString aiPath;
                if (state.isObj)
                    assimpMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                else
                    assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.normalTextures.contains(path)) {
                    material.normalMap = images.normalTextures[path];
                }
                if (images.displacementTextures.contains(path) && state.isObj) {
                    material.displacementMap = images.displacementTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.roughnessTextures.contains(path)) {
                    material.roughnessMap = images.roughnessTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_METALNESS) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_METALNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.metallicTextures.contains(path)) {
                    material.metalnessMap = images.metallicTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_SPECULAR, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.metallicTextures.contains(path) && !material.HasMetalnessMap()) {
                    material.metalnessMap = images.metallicTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0 && !state.isObj) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.displacementTextures.contains(path)) {
                    material.displacementMap = images.displacementTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_EMISSION_COLOR) > 0 ||
                assimpMaterial->GetTextureCount(aiTextureType_EMISSIVE) > 0) {
                aiString aiPath;
                if (assimpMaterial->GetTextureCount(aiTextureType_EMISSION_COLOR) > 0)
                    assimpMaterial->GetTexture(aiTextureType_EMISSION_COLOR, 0, &aiPath);
                else
                    assimpMaterial->GetTexture(aiTextureType_EMISSIVE, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (images.emissiveTextures.contains(path)) {
                    material.emissiveMap = images.emissiveTextures[path];
                }
            }
            
            // Probably foliage
            if (material.HasOpacityMap() ||
                material.opacity < 1.0f) {
                material.twoSided = true;
            }

            material.uvChannel = uvChannel;
            
        }

        void ModelImporter::LoadMaterialImages(ImporterState& state, aiMaterial* material,
            bool hasTangents, int32_t maxTextureResolution) {

            auto& images = state.images;
            auto& directory = state.paths.directoryPath;

            if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0 ||
                material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString aiPath;
                if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
                    material->GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath);
                else
                    material->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));

                if (!images.Contains(MaterialImageType::BaseColor, path)) {
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

                    image->ExpandToChannelCount(4, 255);

                    images.Add(MaterialImageType::BaseColor, path, image);
                    if (!images.Contains(MaterialImageType::Opacity, path) && opacityImage != nullptr) {
                        if (IsImageValid(opacityImage))
                            images.Add(MaterialImageType::Opacity, path, opacityImage);
                    }
                }
            }
            if (material->GetTextureCount(aiTextureType_OPACITY) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_OPACITY, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.Contains(MaterialImageType::Opacity, path)) {
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
                    if (IsImageValid(image))
                        images.Add(MaterialImageType::Opacity, path, image);
                }
            }
            if ((material->GetTextureCount(aiTextureType_NORMALS) > 0 ||
                (material->GetTextureCount(aiTextureType_HEIGHT) > 0 && state.isObj))
                && hasTangents) {
                aiString aiPath;
                if (state.isObj)
                    material->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                else
                    material->GetTexture(aiTextureType_NORMALS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.Contains(MaterialImageType::Normal, path)) {
                    Ref<Common::Image<uint8_t>> displacementImage = nullptr;
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);
                    // Might still be a traditional displacement map
                    if (image->channels == 1 && state.isObj) {
                        displacementImage = image;
                        image = ApplySobelFilter(image);
                    }
                    if (image->channels == 4) {
                        image = image->GetChannelImage(0, 3);
                    }
                    image->ExpandToChannelCount(4, 255);

                    if (IsImageValid(image))
                        images.Add(MaterialImageType::Normal, path, image);
                    if (!images.Contains(MaterialImageType::Displacement, path) && displacementImage != nullptr)
                        images.Add(MaterialImageType::Displacement, path, displacementImage);
                }
            }
            if (material->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));

                if (!images.Contains(MaterialImageType::Roughness, path)) {
                    Ref<Common::Image<uint8_t>> metallicImage = nullptr;
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);

                    // It's common in GLTF that these are stored in one texture
                    if (image->channels > 1) {
                        metallicImage = image->GetChannelImage(2, 1);
                        image = image->GetChannelImage(1, 1);
                    }

                    if (IsImageValid(image))
                        images.Add(MaterialImageType::Roughness, path, image);
                    if (!images.Contains(MaterialImageType::Metallic, path) && metallicImage != nullptr)
                        if (IsImageValid(metallicImage))
                            images.Add(MaterialImageType::Metallic, path, metallicImage);
                }
            }
            if (material->GetTextureCount(aiTextureType_METALNESS) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_METALNESS, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.Contains(MaterialImageType::Metallic, path)) {
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
                    if (IsImageValid(image))
                        images.Add(MaterialImageType::Metallic, path, image);
                }
            }
            if (material->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_SPECULAR, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.Contains(MaterialImageType::Metallic, path)) {
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
                    if (IsImageValid(image))
                        images.Add(MaterialImageType::Metallic, path, image);
                }
            }
            if (material->GetTextureCount(aiTextureType_HEIGHT) > 0 && !state.isObj && hasTangents) {
                aiString aiPath;
                material->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.Contains(MaterialImageType::Displacement, path)) {
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
                    images.Add(MaterialImageType::Displacement, path, image);
                }
            }
            if (material->GetTextureCount(aiTextureType_EMISSION_COLOR) > 0 ||
                material->GetTextureCount(aiTextureType_EMISSIVE) > 0) {
                aiString aiPath;
                if (material->GetTextureCount(aiTextureType_EMISSION_COLOR) > 0)
                    material->GetTexture(aiTextureType_EMISSION_COLOR, 0, &aiPath);
                else
                    material->GetTexture(aiTextureType_EMISSIVE, 0, &aiPath);
                auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
                if (!images.Contains(MaterialImageType::Emissive, path)) {
                    auto image = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);
                    images.Add(MaterialImageType::Emissive, path, image);
                }
            }
        }

        std::vector<Ref<Common::Image<uint8_t>>> ModelImporter::ImagesToTextures(ImporterState& state) {

            auto& images = state.images;
            std::vector<Ref<Common::Image<uint8_t>>> imagesToSave;

            for (const auto& [path, image] : images.baseColorImages) {
                auto texture = std::make_shared<Texture::Texture2D>(image);
                image->fileName = GetMaterialImageImportPath(state, MaterialImageType::BaseColor, path);
                images.baseColorTextures[path] = ResourceManager<Texture::Texture2D>::AddResource(image->fileName, texture);
                imagesToSave.push_back(image);
            }
            for (const auto& [path, image] : images.opacityImages) {
                auto texture = std::make_shared<Texture::Texture2D>(image);
                image->fileName = GetMaterialImageImportPath(state, MaterialImageType::Opacity, path);
                images.opacityTextures[path] = ResourceManager<Texture::Texture2D>::AddResource(image->fileName, texture);
                imagesToSave.push_back(image);
            }
            for (const auto& [path, image] : images.normalImages) {
                auto texture = std::make_shared<Texture::Texture2D>(image);
                image->fileName = GetMaterialImageImportPath(state, MaterialImageType::Normal, path);
                images.normalTextures[path] = ResourceManager<Texture::Texture2D>::AddResource(image->fileName, texture);
                imagesToSave.push_back(image);
            }
            for (const auto& [path, image] : images.roughnessImages) {
                auto texture = std::make_shared<Texture::Texture2D>(image);
                image->fileName = GetMaterialImageImportPath(state, MaterialImageType::Roughness, path);
                images.roughnessTextures[path] = ResourceManager<Texture::Texture2D>::AddResource(image->fileName, texture);
                imagesToSave.push_back(image);
            }
            for (const auto& [path, image] : images.metallicImages) {
                auto texture = std::make_shared<Texture::Texture2D>(image);
                image->fileName = GetMaterialImageImportPath(state, MaterialImageType::Metallic, path);
                images.metallicTextures[path] = ResourceManager<Texture::Texture2D>::AddResource(image->fileName, texture);
                imagesToSave.push_back(image);
            }
            for (const auto& [path, image] : images.displacementImages) {
                auto texture = std::make_shared<Texture::Texture2D>(image);
                image->fileName = GetMaterialImageImportPath(state, MaterialImageType::Displacement, path);
                images.displacementTextures[path] = ResourceManager<Texture::Texture2D>::AddResource(image->fileName, texture);
                imagesToSave.push_back(image);
            }
            for (const auto& [path, image] : images.emissiveImages) {
                auto texture = std::make_shared<Texture::Texture2D>(image);
                image->fileName = GetMaterialImageImportPath(state, MaterialImageType::Emissive, path);
                images.emissiveTextures[path] = ResourceManager<Texture::Texture2D>::AddResource(image->fileName, texture);
                imagesToSave.push_back(image);
            }

            return imagesToSave;

        }

        ModelImporter::Paths ModelImporter::GetPaths(const std::string& filename) {

            auto directoryPath = Common::Path::GetDirectory(filename);

            if (directoryPath.length())
                directoryPath += "/";

            return Paths{
                .filename = filename,
                .directoryPath = directoryPath,
                .meshPath = directoryPath + "meshes/",
                .materialPath = directoryPath + "materials/",
                .texturePath = directoryPath + "textures/",
            };

        }

        std::string ModelImporter::GetMaterialImageImportPath(const ImporterState& state, MaterialImageType type, const std::string& filename) {

            std::string typeName;
            switch (type) {
            case MaterialImageType::BaseColor:
                typeName = "BaseColor"; break;
            case MaterialImageType::Opacity:
                typeName = "Opacity"; break;
            case MaterialImageType::Roughness:
                typeName = "Roughness"; break;
            case MaterialImageType::Metallic:
                typeName = "Metallic"; break;
            case MaterialImageType::Normal:
                typeName = "Normal"; break;
            case MaterialImageType::Displacement:
                typeName = "Displacement"; break;
            case MaterialImageType::Emissive:
                typeName = "Emissive"; break;
            default:
                typeName = "Invalid"; break;
            }

            auto extension = Common::Path::GetFileType(filename); 
            auto fileNameWithoutExtension = Common::Path::GetFileNameWithoutExtension(filename);

            return state.paths.texturePath + fileNameWithoutExtension + "_" + typeName + "." + extension;

        }

        bool ModelImporter::IsImageValid(Ref<Common::Image<uint8_t>>& image) {

            auto& data = image->GetData();

            if (data.empty())
                return false;

            bool invalidImage = true;
            for (int32_t i = image->channels; i < data.size(); i += image->channels) {
                for (int32_t j = 0; j < image->channels; j++)
                    invalidImage &= data[(i - image->channels) + j] == data[i + j];
            }

            return !invalidImage;
        }

    }

}
