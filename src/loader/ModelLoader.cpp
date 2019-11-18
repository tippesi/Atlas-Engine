#include "ModelLoader.h"
#include "ImageLoader.h"
#include "AssetLoader.h"

#include <vector>
#include <limits>

namespace Atlas {

	namespace Loader {

		void ModelLoader::LoadMesh(std::string filename, Mesh::MeshData& meshData) {

			std::string directoryPath(filename);

			size_t directoryPathEnd = directoryPath.find_last_of("/\\");

			if (directoryPath.find_last_of("/\\") != std::string::npos)
				directoryPath = directoryPath.substr(0, directoryPathEnd + 1);
			else
				directoryPath.clear();

			AssetLoader::UnpackFile(filename);

			Assimp::Importer importer;
			importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);

			const aiScene* scene = importer.ReadFile(AssetLoader::GetFullPath(filename),
					aiProcess_CalcTangentSpace |
					aiProcess_JoinIdenticalVertices |
					aiProcess_Triangulate |
					aiProcess_OptimizeGraph |
					aiProcess_OptimizeMeshes |
					aiProcess_RemoveRedundantMaterials |
					aiProcess_GenSmoothNormals |
					aiProcess_LimitBoneWeights |
					aiProcess_ImproveCacheLocality);

			if (!scene) {
#ifdef AE_SHOW_LOG
				AtlasLog("Error processing model %s", importer.GetErrorString());
#endif
				throw AtlasException("Model couldn't be loaded");
			}

			int32_t indexCount = 0;
			int32_t vertexCount = 0;
			int32_t bonesCount = 0;

			bool hasTangents = false;
			bool hasTexCoords = false;

			std::vector<std::vector<aiMesh*>> meshSorted(scene->mNumMaterials);

			for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
				auto mesh = scene->mMeshes[i];

				meshSorted[mesh->mMaterialIndex].push_back(mesh);

				indexCount += (mesh->mNumFaces * 3);
				vertexCount += mesh->mNumVertices;
				bonesCount += mesh->mNumBones;

				hasTexCoords = mesh->mNumUVComponents[0] > 0 ? true : hasTexCoords;
			}

			for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
				if (scene->mMaterials[i]->GetTextureCount(aiTextureType_NORMALS) > 0)
					hasTangents = true;
			}

			if (indexCount > 65535) {
				meshData.indices.SetType(AE_COMPONENT_UNSIGNED_INT);
			}
			else {
				meshData.indices.SetType(AE_COMPONENT_UNSIGNED_SHORT);
			}

			meshData.vertices.SetType(AE_COMPONENT_FLOAT);
			meshData.normals.SetType(AE_COMPONENT_PACKED_FLOAT);
			meshData.texCoords.SetType(AE_COMPONENT_HALF_FLOAT);
			meshData.tangents.SetType(AE_COMPONENT_PACKED_FLOAT);

			meshData.SetIndexCount(indexCount);
			meshData.SetVertexCount(vertexCount);

			if (scene->HasAnimations() || bonesCount > 0) {



			}

			auto min = vec3(std::numeric_limits<float>::max());
			auto max = vec3(-std::numeric_limits<float>::max());

			uint32_t usedFaces = 0;
			uint32_t usedVertices = 0;
			uint32_t loadedVertices = 0;

			uint32_t* indices = new uint32_t[indexCount];
			float* vertices = new float[vertexCount * 3];
			float* normals = new float[vertexCount * 4];
			float* texCoords = hasTexCoords ? new float[vertexCount * 2] : nullptr;
			float* tangents = hasTangents ? new float[vertexCount * 4] : nullptr;

			meshData.subData = std::vector<Mesh::MeshSubData>(scene->mNumMaterials);
			meshData.materials = std::vector<Material>(scene->mNumMaterials);

			for (uint32_t i = 0; i < scene->mNumMaterials; i++) {

				auto& material = meshData.materials[i];
				auto& subData = meshData.subData[i];

				LoadMaterial(scene->mMaterials[i], material, directoryPath);

				subData.material = &material;
				subData.indicesOffset = usedFaces * 3;

				for (auto mesh : meshSorted[i]) {
					// Copy vertices
					for (uint32_t j = 0; j < mesh->mNumVertices; j++) {

						vertices[usedVertices * 3] = mesh->mVertices[j].x;
						vertices[usedVertices * 3 + 1] = mesh->mVertices[j].y;
						vertices[usedVertices * 3 + 2] = mesh->mVertices[j].z;

						max.x = glm::max(mesh->mVertices[j].x, max.x);
						max.y = glm::max(mesh->mVertices[j].y, max.y);
						max.z = glm::max(mesh->mVertices[j].z, max.z);

						min.x = glm::min(mesh->mVertices[j].x, min.x);
						min.y = glm::min(mesh->mVertices[j].y, min.y);
						min.z = glm::min(mesh->mVertices[j].z, min.z);

						vec3 normal = normalize(vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z));

						normals[usedVertices * 4] = normal.x;
						normals[usedVertices * 4 + 1] = normal.y;
						normals[usedVertices * 4 + 2] = normal.z;
						normals[usedVertices * 4 + 3] = 0.0f;

						if (hasTangents && mesh->mTangents != nullptr) {
							vec3 tangent = vec3(mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z);
							tangent = normalize(tangent - normal * dot(normal, tangent));

							vec3 estimatedBitangent = normalize(cross(tangent, normal));
							vec3 correctBitangent = normalize(vec3(mesh->mBitangents[j].x, mesh->mBitangents[j].y,
																   mesh->mBitangents[j].z));

							float dotProduct = dot(estimatedBitangent, correctBitangent);

							tangents[usedVertices * 4] = tangent.x;
							tangents[usedVertices * 4 + 1] = tangent.y;
							tangents[usedVertices * 4 + 2] = tangent.z;
							tangents[usedVertices * 4 + 3] = dotProduct < 0.0f ? -1.0f : 1.0f;
						}

						if (hasTexCoords && mesh->mTextureCoords[0] != nullptr) {
							texCoords[usedVertices * 2] = mesh->mTextureCoords[0][j].x;
							texCoords[usedVertices * 2 + 1] = mesh->mTextureCoords[0][j].y;
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

		}

		void ModelLoader::LoadMaterial(aiMaterial* assimpMaterial, Material& material, std::string directory) {

            aiString name;

			aiColor3D diffuse;
			aiColor3D specular;
			aiColor3D ambient;
			aiColor3D emissive;

			assimpMaterial->Get(AI_MATKEY_NAME, name);
			assimpMaterial->Get(AI_MATKEY_SHININESS, material.specularHardness);
			assimpMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, material.specularIntensity);
			assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
			assimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specular);
			assimpMaterial->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
			assimpMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);

            material.name = std::string(name.C_Str());

			material.diffuseColor = vec3(diffuse.r, diffuse.g, diffuse.b);
			material.specularColor = vec3(specular.r, specular.g, specular.b);
			material.ambientColor = vec3(ambient.r, ambient.g, ambient.b);
			material.emissiveColor = vec3(emissive.r, emissive.g, emissive.b);

			if (assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
				aiString aiPath;
				assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
				auto path = directory + std::string(aiPath.C_Str());
				auto texture = new Texture::Texture2D(path, true);
				material.diffuseMap = texture;
			}
			if (assimpMaterial->GetTextureCount(aiTextureType_NORMALS) > 0) {
				aiString aiPath;
				assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiPath);
				auto path = directory + std::string(aiPath.C_Str());
				auto texture = new Texture::Texture2D(path, false);
				material.normalMap = texture;
			}

		}

	}

}