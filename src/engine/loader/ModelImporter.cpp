#include "ModelImporter.h"
#include "common/MathHelper.h"
#include "ImageLoader.h"
#include "MaterialLoader.h"
#include "MeshLoader.h"
#include "AnimationLoader.h"
#include "AssetLoader.h"
#include "../Log.h"
#include "../common/Path.h"
#include "../graphics/Instance.h"

#include <vector>
#include <limits>
#include <functional>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cctype>

namespace Atlas {

    namespace Loader {

        Ref<Mesh::Mesh> ModelImporter::ImportMesh(const std::string& filename,
            bool saveToDisk, int32_t maxTextureResolution) {

            return ImportMesh(filename, Mesh::MeshMobility::Stationary, saveToDisk,
                maxTextureResolution, ImportTextureFormat::Original);

        }

        Ref<Mesh::Mesh> ModelImporter::ImportMesh(const std::string& filename,
            bool saveToDisk, int32_t maxTextureResolution, ImportTextureFormat textureFormat) {

            return ImportMesh(filename, Mesh::MeshMobility::Stationary, saveToDisk,
                maxTextureResolution, textureFormat);

        }

        Ref<Mesh::Mesh> ModelImporter::ImportMesh(const std::string& filename,
            Mesh::MeshMobility mobility, bool saveToDisk, int32_t maxTextureResolution) {

            return ImportMesh(filename, mobility, saveToDisk, maxTextureResolution, ImportTextureFormat::Original);

        }

        Ref<Mesh::Mesh> ModelImporter::ImportMesh(const std::string& filename,
            Mesh::MeshMobility mobility, bool saveToDisk,
            int32_t maxTextureResolution, ImportTextureFormat textureFormat) {

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
                aiNode* node;
                mat4 transform;
            };

            std::vector<std::vector<AssimpMesh>> meshSorted(state.scene->mNumMaterials);

            std::function<void(aiNode*, mat4)> traverseNodeTree;
            traverseNodeTree = [&](aiNode* node, mat4 accTransform) {
                accTransform = accTransform * glm::transpose(glm::make_mat4(&node->mTransformation.a1));
                for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                    auto meshId = node->mMeshes[i];
                    auto mesh = state.scene->mMeshes[meshId];

                    meshSorted[mesh->mMaterialIndex].push_back({ mesh, node, accTransform });

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

            // Global matrices of the rest pose
            std::vector<mat4> jointMatrices;
            if (bonesCount > 0) {
                ImportSkeleton(state, meshData.skeleton, true);

                std::vector<Mesh::JointPose> pose;
                meshData.skeleton.GetRestPose(pose);
                meshData.skeleton.GetGlobalMatrices(pose, jointMatrices);

                meshData.joints.SetElementCount(vertexCount);
                meshData.weights.SetElementCount(vertexCount);
            }

            auto isSkinned = meshData.IsSkinned();

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

            auto materials = ImportMaterials(state, maxTextureResolution, textureFormat, saveToDisk);

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

                    std::vector<vec4> vertexJoints;
                    std::vector<vec4> vertexWeights;
                    if (isSkinned) {
                        auto jointIdx = meshData.skeleton.GetJointIndex(assimpMesh.node->mName.C_Str());
                        ImportVertexWeights(mesh, meshData.skeleton, std::max(jointIdx, 0), vertexJoints, vertexWeights);

                        // Skinned meshes are in the bind space of the skeleton, rigid meshes are bound to the joint of their node
                        if (mesh->mNumBones > 0)
                            matrix = GetBindSpaceMatrix(mesh, meshData.skeleton);
                        else if (jointIdx >= 0)
                            matrix = glm::inverse(jointMatrices[jointIdx] * meshData.skeleton.joints[jointIdx].inverseBindMatrix) * matrix;
                    }

                    // Copy vertices
                    for (uint32_t j = 0; j < mesh->mNumVertices; j++) {

                        vec3 vertex = vec3(matrix * vec4(mesh->mVertices[j].x, 
                            mesh->mVertices[j].y, mesh->mVertices[j].z, 1.0f));

                        vertices[usedVertices] = vertex;

                        if (isSkinned) {
                            meshData.joints[usedVertices] = vertexJoints[j];
                            meshData.weights[usedVertices] = vertexWeights[j];
                        }

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
                            colors[usedVertices] = vec4(mesh->mColors[0][j].r, mesh->mColors[0][j].g,
                                mesh->mColors[0][j].b, mesh->mColors[0][j].a);
                        }
                        else if (hasVertexColors && mesh->mColors[0] == nullptr) {
                            colors[usedVertices] = vec4(1.0f);
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

            // Skinned meshes are rendered in their rest pose without an animation
            if (isSkinned) {
                ImportJointBounds(meshData);
                meshData.skeleton.UpdateRestPose();
                meshData.aabb = meshData.skeleton.restAABB;
            }
            

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
            bool invertUVs, int32_t maxTextureResolution, ImportTextureFormat textureFormat) {

            ImporterState state;
            InitImporterState(state, filename, false);

            auto materials = ImportMaterials(state, maxTextureResolution, textureFormat, saveToDisk);

            struct MeshInfo {
                ResourceHandle<Mesh::Mesh> mesh;
                vec3 offset;
            };

            std::vector<MeshInfo> meshes;
            meshes.resize(state.scene->mNumMeshes);

            // All skinned meshes share one skeleton
            Mesh::Skeleton skeleton;
            for (uint32_t i = 0; i < state.scene->mNumMeshes; i++) {
                if (state.scene->mMeshes[i]->mNumBones > 0) {
                    ImportSkeleton(state, skeleton, false);
                    break;
                }
            }
            
            std::mutex vertexColorMutex;

            JobGroup group{ "Mesh import" };
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

                auto isSkinned = assimpMesh->mNumBones > 0;
                if (isSkinned) {
                    meshData.skeleton = skeleton;
                    // Each mesh has its own skeleton, which can use the offset matrices of this mesh
                    for (uint32_t j = 0; j < assimpMesh->mNumBones; j++) {
                        auto bone = assimpMesh->mBones[j];
                        auto jointIdx = skeleton.GetJointIndex(bone->mName.C_Str());
                        if (jointIdx >= 0)
                            meshData.skeleton.joints[jointIdx].inverseBindMatrix =
                                glm::transpose(glm::make_mat4(&bone->mOffsetMatrix.a1));
                    }
                    ImportVertexWeights(assimpMesh, skeleton, 0, meshData.joints.Get(), meshData.weights.Get());
                }

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

                // Skinned meshes stay in their bind space
                auto offset = isSkinned ? vec3(0.0f) : (max + min) * 0.5f;
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

                if (isSkinned) {
                    ImportJointBounds(meshData);
                    meshData.skeleton.UpdateRestPose();
                    meshData.aabb = meshData.skeleton.restAABB;
                }

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

            std::map<std::string, aiCamera*> cameraMap;
            for (uint32_t i = 0; i < state.scene->mNumCameras; i++) {
                auto camera = state.scene->mCameras[i];
                cameraMap[camera->mName.C_Str()] = camera;
            }

            auto rootEntity = scene->CreateEntity();
            rootEntity.AddComponent<NameComponent>("Root");
            auto& rootHierarchy = rootEntity.AddComponent<HierarchyComponent>();
            rootHierarchy.root = true;

            std::function<void(aiNode*, Scene::Entity, mat4)> traverseNodeTree;
            traverseNodeTree = [&](aiNode* node, Scene::Entity parentEntity, mat4 parentMatrix) {
                auto nodeTransform = glm::transpose(glm::make_mat4(&node->mTransformation.a1));

                auto& parentNameComp = parentEntity.GetComponent<NameComponent>();

                for (uint32_t i = 0; i < node->mNumMeshes; i++) {
                    auto meshId = node->mMeshes[i];
                    auto& meshInfo = meshes[meshId];

                    auto instanceTransform = glm::translate(meshInfo.offset);
                    // Skinned meshes are posed in the space of the model, independent of their node
                    auto transform = meshInfo.mesh->data.IsSkinned() ? glm::inverse(parentMatrix) :
                        nodeTransform * instanceTransform;

                    auto entity = scene->CreatePrefab<Scene::Prefabs::MeshInstance>(
                        meshInfo.mesh, transform, makeMeshesStatic);

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

                if (cameraMap.contains(node->mName.C_Str())) {
                    auto camera = cameraMap[node->mName.C_Str()];

                    float verticalFOV = glm::degrees(camera->mHorizontalFOV) / camera->mAspect;
                    vec3 position = vec3(camera->mPosition.x, camera->mPosition.y, camera->mPosition.z);
                    vec2 rotation = vec2(0.0f);
                    parentEntity.AddComponent<CameraComponent>(verticalFOV, camera->mAspect, camera->mClipPlaneNear,
                        camera->mClipPlaneFar, position, rotation);
                }

                for (uint32_t i = 0; i < node->mNumChildren; i++) {
                    auto nodeEntity = scene->CreatePrefab<Scene::Prefabs::Node>(node->mChildren[i]->mName.C_Str(), nodeTransform);
                    auto const& hierarchy = nodeEntity.GetComponent<HierarchyComponent>();

                    parentEntity.GetComponent<HierarchyComponent>().AddChild(nodeEntity);

                    traverseNodeTree(node->mChildren[i], nodeEntity, parentMatrix * nodeTransform);
                }
            };

            traverseNodeTree(state.scene->mRootNode, rootEntity, mat4(1.0f));

            state.importer.FreeScene();

            return scene;

        }

        std::vector<Ref<Animation::Animation>> ModelImporter::ImportAnimations(const std::string& filename,
            bool saveToDisk) {

            ImporterState state;
            InitImporterState(state, filename, false);

            std::vector<Ref<Animation::Animation>> animations;
            for (uint32_t i = 0; i < state.scene->mNumAnimations; i++) {
                auto assimpAnimation = state.scene->mAnimations[i];

                auto ticksPerSecond = assimpAnimation->mTicksPerSecond > 0.0 ?
                    float(assimpAnimation->mTicksPerSecond) : 25.0f;

                auto animation = CreateRef<Animation::Animation>();
                animation->name = std::string(assimpAnimation->mName.C_Str());
                if (animation->name.empty())
                    animation->name = "Animation" + std::to_string(i);
                animation->duration = float(assimpAnimation->mDuration) / ticksPerSecond;

                for (uint32_t j = 0; j < assimpAnimation->mNumChannels; j++) {
                    auto assimpChannel = assimpAnimation->mChannels[j];

                    Animation::Channel channel;
                    channel.jointName = std::string(assimpChannel->mNodeName.C_Str());

                    for (uint32_t k = 0; k < assimpChannel->mNumPositionKeys; k++) {
                        const auto& key = assimpChannel->mPositionKeys[k];
                        channel.positionTimes.push_back(float(key.mTime) / ticksPerSecond);
                        channel.positions.push_back(vec3(key.mValue.x, key.mValue.y, key.mValue.z));
                    }
                    for (uint32_t k = 0; k < assimpChannel->mNumRotationKeys; k++) {
                        const auto& key = assimpChannel->mRotationKeys[k];
                        channel.rotationTimes.push_back(float(key.mTime) / ticksPerSecond);
                        channel.rotations.push_back(quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z));
                    }
                    for (uint32_t k = 0; k < assimpChannel->mNumScalingKeys; k++) {
                        const auto& key = assimpChannel->mScalingKeys[k];
                        channel.scaleTimes.push_back(float(key.mTime) / ticksPerSecond);
                        channel.scales.push_back(vec3(key.mValue.x, key.mValue.y, key.mValue.z));
                    }

                    animation->channels.push_back(channel);
                }

                animations.push_back(animation);

                if (saveToDisk) {
                    auto animationFilename = GetAnimationFilename(filename, animation->name);
                    Log::Message("Imported animation " + animationFilename);
                    Loader::AnimationLoader::SaveAnimation(animation, animationFilename);
                }
            }

            state.importer.FreeScene();

            return animations;

        }

        std::string ModelImporter::GetAnimationFilename(const std::string& filename, const std::string& animationName) {

            auto name = animationName;
            std::replace_if(name.begin(), name.end(), [](unsigned char c) { return !std::isalnum(c) && c != '_' && c != '-'; }, '_');

            return GetPaths(filename).animationPath + Common::Path::GetFileNameWithoutExtension(filename) +
                "_" + name + ".aeanimation";

        }

        void ModelImporter::ImportSkeleton(ImporterState& state, Mesh::Skeleton& skeleton, bool rigidMeshJoints) {

            std::map<std::string, mat4> inverseBindMatrices;
            for (uint32_t i = 0; i < state.scene->mNumMeshes; i++) {
                auto mesh = state.scene->mMeshes[i];
                for (uint32_t j = 0; j < mesh->mNumBones; j++) {
                    auto bone = mesh->mBones[j];
                    // The offset matrix is relative to the mesh, other meshes are transformed into the space of the first one
                    inverseBindMatrices.try_emplace(bone->mName.C_Str(), glm::transpose(glm::make_mat4(&bone->mOffsetMatrix.a1)));
                }
            }

            // Joints are all bones, all nodes with rigid meshes and their ancestors
            std::set<aiNode*> jointNodes;
            std::function<bool(aiNode*)> findJointNodes;
            findJointNodes = [&](aiNode* node) -> bool {
                bool isJoint = inverseBindMatrices.contains(node->mName.C_Str());
                for (uint32_t i = 0; i < node->mNumMeshes && rigidMeshJoints; i++) {
                    isJoint |= state.scene->mMeshes[node->mMeshes[i]]->mNumBones == 0;
                }
                for (uint32_t i = 0; i < node->mNumChildren; i++) {
                    isJoint |= findJointNodes(node->mChildren[i]);
                }
                if (isJoint)
                    jointNodes.insert(node);
                return isJoint;
            };
            findJointNodes(state.scene->mRootNode);

            std::function<void(aiNode*, int32_t, mat4)> addJoints;
            addJoints = [&](aiNode* node, int32_t parentIdx, mat4 parentMatrix) {
                if (!jointNodes.contains(node))
                    return;

                aiVector3D scale, position;
                aiQuaternion rotation;
                node->mTransformation.Decompose(scale, rotation, position);

                Mesh::Joint joint;
                joint.name = std::string(node->mName.C_Str());
                joint.parentIdx = parentIdx;
                joint.position = vec3(position.x, position.y, position.z);
                joint.rotation = quat(rotation.w, rotation.x, rotation.y, rotation.z);
                joint.scale = vec3(scale.x, scale.y, scale.z);

                auto globalMatrix = parentMatrix * glm::transpose(glm::make_mat4(&node->mTransformation.a1));
                if (inverseBindMatrices.contains(joint.name))
                    joint.inverseBindMatrix = inverseBindMatrices[joint.name];
                else
                    joint.inverseBindMatrix = glm::inverse(globalMatrix);

                auto jointIdx = int32_t(skeleton.joints.size());
                skeleton.joints.push_back(joint);

                for (uint32_t i = 0; i < node->mNumChildren; i++) {
                    addJoints(node->mChildren[i], jointIdx, globalMatrix);
                }
            };
            addJoints(state.scene->mRootNode, -1, mat4(1.0f));

        }

        void ModelImporter::ImportVertexWeights(const aiMesh* mesh, const Mesh::Skeleton& skeleton, int32_t defaultJointIdx,
            std::vector<vec4>& joints, std::vector<vec4>& weights) {

            joints = std::vector<vec4>(mesh->mNumVertices, vec4(0.0f));
            weights = std::vector<vec4>(mesh->mNumVertices, vec4(0.0f));
            std::vector<int32_t> weightCounts(mesh->mNumVertices, 0);

            for (uint32_t i = 0; i < mesh->mNumBones; i++) {
                auto bone = mesh->mBones[i];
                auto jointIdx = skeleton.GetJointIndex(bone->mName.C_Str());
                if (jointIdx < 0)
                    continue;

                for (uint32_t j = 0; j < bone->mNumWeights; j++) {
                    const auto& vertexWeight = bone->mWeights[j];
                    auto& count = weightCounts[vertexWeight.mVertexId];
                    if (count >= 4 || vertexWeight.mWeight <= 0.0f)
                        continue;

                    joints[vertexWeight.mVertexId][count] = float(jointIdx);
                    weights[vertexWeight.mVertexId][count] = vertexWeight.mWeight;
                    count++;
                }
            }

            for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
                auto weightSum = glm::dot(weights[i], vec4(1.0f));
                if (weightSum > 0.0f) {
                    weights[i] /= weightSum;
                }
                else {
                    joints[i] = vec4(float(defaultJointIdx), 0.0f, 0.0f, 0.0f);
                    weights[i] = vec4(1.0f, 0.0f, 0.0f, 0.0f);
                }
            }

        }

        mat4 ModelImporter::GetBindSpaceMatrix(const aiMesh* mesh, const Mesh::Skeleton& skeleton) {

            for (uint32_t i = 0; i < mesh->mNumBones; i++) {
                auto bone = mesh->mBones[i];
                auto jointIdx = skeleton.GetJointIndex(bone->mName.C_Str());
                if (jointIdx < 0)
                    continue;

                auto inverseBindMatrix = glm::transpose(glm::make_mat4(&bone->mOffsetMatrix.a1));
                return glm::inverse(skeleton.joints[jointIdx].inverseBindMatrix) * inverseBindMatrix;
            }

            return mat4(1.0f);

        }

        void ModelImporter::ImportJointBounds(Mesh::MeshData& meshData) {

            auto& joints = meshData.skeleton.joints;
            for (size_t i = 0; i < size_t(meshData.GetVertexCount()); i++) {
                const auto& vertex = meshData.vertices[i];
                for (int32_t j = 0; j < 4; j++) {
                    if (meshData.weights[i][j] <= 0.0f)
                        continue;

                    auto& joint = joints[int32_t(meshData.joints[i][j])];
                    auto jointVertex = vec3(joint.inverseBindMatrix * vec4(vertex, 1.0f));
                    if (joint.hasVertices)
                        joint.aabb.Grow(jointVertex);
                    else
                        joint.aabb = Volume::AABB(jointVertex, jointVertex);
                    joint.hasVertices = true;
                }
            }

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

        std::vector<ResourceHandle<Material>> ModelImporter::ImportMaterials(ImporterState& state, int32_t maxTextureResolution,
            ImportTextureFormat textureFormat, bool saveToDisk) {

            JobGroup group{ "Material import" };
            JobSystem::ExecuteMultiple(group, state.scene->mNumMaterials, [&](const JobData& data) {
                LoadMaterialImages(state, state.scene->mMaterials[data.idx], true, maxTextureResolution);
                });
            JobSystem::Wait(group);

            if (textureFormat == ImportTextureFormat::KTX2)
                ConvertImagesToKtx(state);

            auto imagesToSave = ImagesToTextures(state);

            if (saveToDisk) {
                JobSystem::ExecuteMultiple(group, int32_t(imagesToSave.size()), [&](const JobData& data) {
                    auto& image = imagesToSave[data.idx];
                    if (state.images.ktxImages.contains(image))
                        KtxLoader::SaveImage(state.images.ktxImages.at(image), image->fileName);
                    else
                        ImageLoader::SaveImage(image, image->fileName);
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
                materials.push_back(handle);

                if (saveToDisk)
                    Loader::MaterialLoader::SaveMaterial(material, materialFilename);
            }

            return materials;

        }

        void ModelImporter::LoadMaterial(ImporterState& state, aiMaterial* assimpMaterial, Material& material) {

            auto& images = state.images;

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

            material.roughness = Helper::Math::Saturate(material.roughness);
            material.metalness = Helper::Math::Saturate(material.metalness);

            if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0 ||
                assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString aiPath;
                aiTextureMapping aiMapping;
                if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
                    assimpMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath, &aiMapping, &uvChannel);
                else
                    assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath, &aiMapping, &uvChannel);
                auto path = GetMaterialImagePath(state, aiPath);
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
                auto path = GetMaterialImagePath(state, aiPath);
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
                auto path = GetMaterialImagePath(state, aiPath);
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
                auto path = GetMaterialImagePath(state, aiPath);
                if (images.roughnessTextures.contains(path)) {
                    material.roughnessMap = images.roughnessTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_METALNESS) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_METALNESS, 0, &aiPath);
                auto path = GetMaterialImagePath(state, aiPath);
                if (images.metallicTextures.contains(path)) {
                    material.metalnessMap = images.metallicTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_SPECULAR, 0, &aiPath);
                auto path = GetMaterialImagePath(state, aiPath);
                if (images.metallicTextures.contains(path) && !material.HasMetalnessMap()) {
                    material.metalnessMap = images.metallicTextures[path];
                }
            }
            if (assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0 && !state.isObj) {
                aiString aiPath;
                assimpMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                auto path = GetMaterialImagePath(state, aiPath);
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
                auto path = GetMaterialImagePath(state, aiPath);
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

            if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0 ||
                material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString aiPath;
                if (material->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
                    material->GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath);
                else
                    material->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
                auto path = GetMaterialImagePath(state, aiPath);

                if (!images.Contains(MaterialImageType::BaseColor, path)) {
                    Ref<Common::Image<uint8_t>> opacityImage = nullptr;
                    auto image = LoadMaterialImage(state, aiPath, true, 0, maxTextureResolution);

                    if (image && image->channels == 1) {
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
                    else if (image && image->channels == 4) {
                        opacityImage = image->GetChannelImage(3, 1);
                        image = image->GetChannelImage(0, 3);
                    }

                    if (image) {
                        image->ExpandToChannelCount(4, 255);
                        images.Add(MaterialImageType::BaseColor, path, image);
                    }
                    if (!images.Contains(MaterialImageType::Opacity, path) && opacityImage != nullptr) {
                        if (IsImageValid(opacityImage))
                            images.Add(MaterialImageType::Opacity, path, opacityImage);
                    }
                }
            }
            if (material->GetTextureCount(aiTextureType_OPACITY) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_OPACITY, 0, &aiPath);
                auto path = GetMaterialImagePath(state, aiPath);
                if (!images.Contains(MaterialImageType::Opacity, path)) {
                    auto image = LoadMaterialImage(state, aiPath, false, 1, maxTextureResolution);
                    if (image && IsImageValid(image))
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
                auto path = GetMaterialImagePath(state, aiPath);
                auto image = images.Contains(MaterialImageType::Normal, path) ? nullptr :
                    LoadMaterialImage(state, aiPath, false, 0, maxTextureResolution);
                if (image) {
                    Ref<Common::Image<uint8_t>> displacementImage = nullptr;
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
                auto path = GetMaterialImagePath(state, aiPath);
                auto image = images.Contains(MaterialImageType::Roughness, path) ? nullptr :
                    LoadMaterialImage(state, aiPath, false, 0, maxTextureResolution);
                if (image) {
                    Ref<Common::Image<uint8_t>> metallicImage = nullptr;

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
                auto path = GetMaterialImagePath(state, aiPath);
                if (!images.Contains(MaterialImageType::Metallic, path)) {
                    auto image = LoadMaterialImage(state, aiPath, false, 1, maxTextureResolution);
                    if (image && IsImageValid(image))
                        images.Add(MaterialImageType::Metallic, path, image);
                }
            }
            if (material->GetTextureCount(aiTextureType_SPECULAR) > 0) {
                aiString aiPath;
                material->GetTexture(aiTextureType_SPECULAR, 0, &aiPath);
                auto path = GetMaterialImagePath(state, aiPath);
                if (!images.Contains(MaterialImageType::Metallic, path)) {
                    auto image = LoadMaterialImage(state, aiPath, false, 1, maxTextureResolution);
                    if (image && IsImageValid(image))
                        images.Add(MaterialImageType::Metallic, path, image);
                }
            }
            if (material->GetTextureCount(aiTextureType_HEIGHT) > 0 && !state.isObj && hasTangents) {
                aiString aiPath;
                material->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
                auto path = GetMaterialImagePath(state, aiPath);
                if (!images.Contains(MaterialImageType::Displacement, path)) {
                    auto image = LoadMaterialImage(state, aiPath, false, 1, maxTextureResolution);
                    if (image)
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
                auto path = GetMaterialImagePath(state, aiPath);
                if (!images.Contains(MaterialImageType::Emissive, path)) {
                    auto image = LoadMaterialImage(state, aiPath, false, 0, maxTextureResolution);
                    if (image)
                        images.Add(MaterialImageType::Emissive, path, image);
                }
            }
        }

        std::string ModelImporter::GetMaterialImagePath(const ImporterState& state, const aiString& aiPath) {

            auto [texture, textureIdx] = state.scene->GetEmbeddedTextureAndIndex(aiPath.C_Str());
            if (!texture)
                return Common::Path::Normalize(state.paths.directoryPath + std::string(aiPath.C_Str()));

            // Embedded textures get a file name next to the model, compressed ones keep their format
            // The index keeps the names unique, embedded file names don't need to be
            auto name = (texture->mFilename.length > 0 ? Common::Path::GetFileNameWithoutExtension(texture->mFilename.C_Str()) :
                Common::Path::GetFileNameWithoutExtension(state.paths.filename) + "_Texture") + std::to_string(textureIdx);
            auto extension = texture->mHeight == 0 && texture->achFormatHint[0] != '\0' ?
                std::string(texture->achFormatHint) : std::string("png");

            return Common::Path::Normalize(state.paths.directoryPath + name + "." + extension);

        }

        Ref<Common::Image<uint8_t>> ModelImporter::LoadMaterialImage(const ImporterState& state, const aiString& aiPath,
            bool colorSpaceConversion, int32_t forceChannels, int32_t maxTextureResolution) {

            auto path = GetMaterialImagePath(state, aiPath);
            auto texture = state.scene->GetEmbeddedTexture(aiPath.C_Str());

            Ref<Common::Image<uint8_t>> image;
            if (!texture) {
                image = ImageLoader::LoadImage<uint8_t>(path, colorSpaceConversion, forceChannels, maxTextureResolution);
            }
            else if (texture->mHeight == 0) {
                // Compressed textures contain the content of the image file
                auto data = reinterpret_cast<const char*>(texture->pcData);
                std::vector<char> buffer(data, data + texture->mWidth);
                image = ImageLoader::LoadImageFromMemory<uint8_t>(buffer, path, colorSpaceConversion, forceChannels, maxTextureResolution);
            }
            else {
                // Uncompressed textures are converted to a PNG, such that they are loaded like all other images
                std::vector<uint8_t> texels(size_t(texture->mWidth) * size_t(texture->mHeight) * 4);
                for (size_t i = 0; i < texels.size() / 4; i++) {
                    const auto& texel = texture->pcData[i];
                    texels[i * 4 + 0] = texel.r;
                    texels[i * 4 + 1] = texel.g;
                    texels[i * 4 + 2] = texel.b;
                    texels[i * 4 + 3] = texel.a;
                }

                std::vector<char> buffer;
                auto writeToBuffer = [](void* context, void* data, int32_t size) {
                    auto buffer = static_cast<std::vector<char>*>(context);
                    auto bytes = static_cast<char*>(data);
                    buffer->insert(buffer->end(), bytes, bytes + size);
                    };
                stbi_write_png_to_func(writeToBuffer, &buffer, int32_t(texture->mWidth), int32_t(texture->mHeight),
                    4, texels.data(), int32_t(texture->mWidth) * 4);
                image = ImageLoader::LoadImageFromMemory<uint8_t>(buffer, path, colorSpaceConversion, forceChannels, maxTextureResolution);
            }

            // Missing or broken images are left out of the material
            if (image->GetData().empty())
                return nullptr;

            return image;

        }

        void ModelImporter::ConvertImagesToKtx(ImporterState& state) {

            auto& images = state.images;
            std::vector<Ref<Common::Image<uint8_t>>> imagesToConvert;

            for (const auto imageMap : { &images.baseColorImages, &images.opacityImages, &images.normalImages,
                &images.roughnessImages, &images.metallicImages, &images.displacementImages, &images.emissiveImages }) {
                for (const auto& [path, image] : *imageMap)
                    imagesToConvert.push_back(image);
            }

            std::vector<Ref<KtxImage>> ktxImages(imagesToConvert.size());

            JobGroup group{ "Image conversion" };
            JobSystem::ExecuteMultiple(group, int32_t(imagesToConvert.size()), [&](const JobData& data) {
                try {
                    ktxImages[data.idx] = KtxLoader::ConvertImage(imagesToConvert[data.idx]);
                }
                catch (const std::exception& exception) {
                    Log::Error("Couldn't convert image " + imagesToConvert[data.idx]->fileName + ": " + exception.what());
                }
                });
            JobSystem::Wait(group);

            for (size_t i = 0; i < imagesToConvert.size(); i++) {
                if (ktxImages[i])
                    images.ktxImages[imagesToConvert[i]] = ktxImages[i];
            }

        }

        std::vector<Ref<Common::Image<uint8_t>>> ModelImporter::ImagesToTextures(ImporterState& state) {

            auto& images = state.images;
            std::vector<Ref<Common::Image<uint8_t>>> imagesToSave;

            auto addTexture = [&](MaterialImageType type, const std::string& path,
                const Ref<Common::Image<uint8_t>>& image) -> ResourceHandle<Texture::Texture2D> {
                Ref<Texture::Texture2D> texture;
                if (images.ktxImages.contains(image)) {
                    image->fileName = GetMaterialImageImportPath(state, type, path, ImportTextureFormat::KTX2);
                    texture = std::make_shared<Texture::Texture2D>(images.ktxImages[image]);
                }
                else {
                    image->fileName = GetMaterialImageImportPath(state, type, path, ImportTextureFormat::Original);
                    texture = std::make_shared<Texture::Texture2D>(image);
                }
                imagesToSave.push_back(image);
                return ResourceManager<Texture::Texture2D>::AddResource(image->fileName, texture);
                };

            for (const auto& [path, image] : images.baseColorImages)
                images.baseColorTextures[path] = addTexture(MaterialImageType::BaseColor, path, image);
            for (const auto& [path, image] : images.opacityImages)
                images.opacityTextures[path] = addTexture(MaterialImageType::Opacity, path, image);
            for (const auto& [path, image] : images.normalImages)
                images.normalTextures[path] = addTexture(MaterialImageType::Normal, path, image);
            for (const auto& [path, image] : images.roughnessImages)
                images.roughnessTextures[path] = addTexture(MaterialImageType::Roughness, path, image);
            for (const auto& [path, image] : images.metallicImages)
                images.metallicTextures[path] = addTexture(MaterialImageType::Metallic, path, image);
            for (const auto& [path, image] : images.displacementImages)
                images.displacementTextures[path] = addTexture(MaterialImageType::Displacement, path, image);
            for (const auto& [path, image] : images.emissiveImages)
                images.emissiveTextures[path] = addTexture(MaterialImageType::Emissive, path, image);

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
                .animationPath = directoryPath + "animations/",
            };

        }

        std::string ModelImporter::GetMaterialImageImportPath(const ImporterState& state, MaterialImageType type,
            const std::string& filename, ImportTextureFormat textureFormat) {

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

            auto extension = textureFormat == ImportTextureFormat::KTX2 ? "ktx2" : Common::Path::GetFileType(filename);
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
