#include "ModelLoader.h"
#include "ImageLoader.h"
#include "AssetLoader.h"
#include "../Log.h"
#include "../common/Path.h"

#include <vector>
#include <limits>
#include <functional>

namespace Atlas {

	namespace Loader {

		Mesh::MeshData ModelLoader::LoadMesh(std::string filename,
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

			meshData.subData = std::vector<Mesh::MeshSubData>(scene->mNumMaterials);
			meshData.materials = std::vector<Material>(scene->mNumMaterials);

			for (uint32_t i = 0; i < scene->mNumMaterials; i++) {

				auto& material = meshData.materials[i];
				auto& subData = meshData.subData[i];

				LoadMaterial(scene->mMaterials[i], material,
                             directoryPath, isObj, maxTextureResolution);

				subData.material = &material;
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

			meshData.aabb = Volume::AABB(min, max);

			meshData.indices.Set(indices);
			meshData.vertices.Set(vertices);
			meshData.normals.Set(normals);

			if (hasTexCoords)
				meshData.texCoords.Set(texCoords);
			if (hasTangents)
				meshData.tangents.Set(tangents);

			meshData.filename = filename;

			return meshData;

		}

		void ModelLoader::LoadMaterial(aiMaterial* assimpMaterial, Material& material,
			std::string directory, bool isObj, int32_t maxTextureResolution) {

			bool roughnessMetalnessTexture = false;

			aiString name;

			aiColor3D diffuse;
			aiColor3D emissive;
			aiColor3D specular;
			float specularHardness;
			float specularIntensity;

			assimpMaterial->Get(AI_MATKEY_NAME, name);
			assimpMaterial->Get(AI_MATKEY_SHININESS, specularHardness);
			assimpMaterial->Get(AI_MATKEY_OPACITY, material.opacity);
			assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			assimpMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
			assimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specular);

			material.name = std::string(name.C_Str());

			material.baseColor = vec3(diffuse.r, diffuse.g, diffuse.b);
			material.emissiveColor = vec3(emissive.r, emissive.g, emissive.b);

			material.displacementScale = 0.01f;

			// Avoid NaN
			specularHardness = glm::max(1.0f, specularHardness);
			material.roughness = glm::clamp(powf(1.0f / (0.5f * specularHardness + 1.0f), 0.25f), 0.0f, 1.0f);
			material.ao = 1.0f;

			specularIntensity = glm::max(specular.r, glm::max(specular.g, specular.b));
			material.metalness = glm::clamp(specularIntensity, 0.0f, 1.0f);
			
			if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0 ||
				assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
				aiString aiPath;
				if (assimpMaterial->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
					assimpMaterial->GetTexture(aiTextureType_BASE_COLOR, 0, &aiPath);
				else
					assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
				auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
				auto image = ImageLoader::LoadImage<uint8_t>(path, true, 0, maxTextureResolution);

				material.baseColorMap = new Texture::Texture2D(image.width, image.height, AE_RGB8,
					GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, true, true);

				std::vector <uint8_t> data(image.width * image.height * 3);
				if (image.channels == 1) {
					auto imageData = image.GetData();
					for (size_t i = 0; i < data.size(); i += 3) {
						data[i + 0] = imageData[i / 3];
						data[i + 1] = imageData[i / 3];
						data[i + 2] = imageData[i / 3];
					}
				}
				else if (image.channels == 3) {
					data = image.GetData();
				}
				else if (image.channels == 4) {
					data = image.GetChannelData(0, 3);
					auto opacityData = image.GetChannelData(3, 1);
					material.opacityMap = new Texture::Texture2D(image.width, image.height, AE_R8,
						GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, true, true);
					material.opacityMap->SetData(opacityData);
					material.opacityMapPath = path;
				}
				material.baseColorMap->SetData(data);
				material.baseColorMapPath = path;
			}
			if (assimpMaterial->GetTextureCount(aiTextureType_OPACITY) > 0) {
				aiString aiPath;
				assimpMaterial->GetTexture(aiTextureType_OPACITY, 0, &aiPath);
				auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
				auto image = ImageLoader::LoadImage<uint8_t>(path, true, 1, maxTextureResolution);
				material.opacityMap = new Texture::Texture2D(image.width, image.height, AE_R8,
					GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, true, true);
				material.opacityMap->SetData(image.GetData());
				material.opacityMapPath = path;
			}
			if (assimpMaterial->GetTextureCount(aiTextureType_NORMALS) > 0 ||
				(assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0 && isObj)) {
				aiString aiPath;
				if (isObj)
					assimpMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
				else
					assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiPath);
				auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
				auto image = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);
				auto texture = new Texture::Texture2D(image);
				// Might still be a traditional displacement map
				if (texture->channels == 1 && isObj) {
					material.displacementMap = texture;
					material.displacementMapPath = path;
					image = ApplySobelFilter(image, 0.5f);
					material.normalMap = new Texture::Texture2D(image);
					material.normalMapPath = path;
				}
				else {
					material.normalMap = texture;
					material.normalMapPath = path;
				}
			}
			if (assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0) {
				aiString aiPath;
				assimpMaterial->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &aiPath);
				auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
				auto image = ImageLoader::LoadImage<uint8_t>(path, false, 0, maxTextureResolution);
				if (image.channels > 1) {
					auto roughnessData = image.GetChannelData(1, 1);
					auto metalnessData = image.GetChannelData(2, 1);
					material.roughnessMap = new Texture::Texture2D(image.width, image.height, AE_R8,
						GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, true, true);
					material.roughnessMap->SetData(roughnessData);
					material.metalnessMap = new Texture::Texture2D(image.width, image.height, AE_R8,
						GL_REPEAT, GL_LINEAR_MIPMAP_LINEAR, true, true);
					material.metalnessMap->SetData(metalnessData);
					material.metalnessMapPath = path;
					roughnessMetalnessTexture = true;
				}
				else {
					material.roughnessMap = new Texture::Texture2D(image, true, true);
				}
				material.roughnessMapPath = path;
			}
			if (assimpMaterial->GetTextureCount(aiTextureType_METALNESS) > 0 && !roughnessMetalnessTexture) {
				aiString aiPath;
				assimpMaterial->GetTexture(aiTextureType_METALNESS, 0, &aiPath);
				auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
				auto image = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
				material.metalnessMap = new Texture::Texture2D(image, true, true);
				material.metalnessMapPath = path;
			}
			if (assimpMaterial->GetTextureCount(aiTextureType_SPECULAR) > 0 && !material.metalnessMap) {
				aiString aiPath;
				assimpMaterial->GetTexture(aiTextureType_SPECULAR, 0, &aiPath);
				auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
				auto image = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
				auto texture = new Texture::Texture2D(image, true, true);
				material.metalnessMap = texture;
				material.metalnessMapPath = path;
			}
			if (assimpMaterial->GetTextureCount(aiTextureType_HEIGHT) > 0 && !isObj) {
				aiString aiPath;
				assimpMaterial->GetTexture(aiTextureType_HEIGHT, 0, &aiPath);
				auto path = Common::Path::Normalize(directory + std::string(aiPath.C_Str()));
				auto image = ImageLoader::LoadImage<uint8_t>(path, false, 1, maxTextureResolution);
				material.displacementMap = new Texture::Texture2D(image, true, true);
				material.displacementMapPath = path;
			}

			// Probably foliage
			if (material.HasOpacityMap() ||
				material.opacity < 1.0f) {
				material.twoSided = true;
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