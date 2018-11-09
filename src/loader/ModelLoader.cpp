#include "ModelLoader.h"

#include <vector>

MeshData* ModelLoader::LoadMesh(const char* filename) {

	string directoryPath(filename);

	size_t directoryPathEnd = directoryPath.find_last_of("/\\");

	if (directoryPath.find_last_of("/\\") != string::npos)
		directoryPath = directoryPath.substr(0, directoryPathEnd + 1);
	else
		directoryPath.clear();

	Assimp::Importer importer;
	importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);

	const aiScene* scene = importer.ReadFile(filename,
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices |
		aiProcess_Triangulate |
		aiProcess_OptimizeGraph |
		aiProcess_OptimizeMeshes |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_GenSmoothNormals |
		aiProcess_LimitBoneWeights |
		aiProcess_ImproveCacheLocality);

	MeshData* meshData = new MeshData();

	int32_t indexCount = 0;
	int32_t vertexCount = 0;
	int32_t bonesCount = 0;

	bool hasTangents = false;
	bool hasTexCoords = false;

	vector<vector<aiMesh*>> meshSorted(scene->mNumMaterials);

	for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
		auto mesh = scene->mMeshes[i];

		meshSorted[mesh->mMaterialIndex].push_back(mesh);

		indexCount += (mesh->mNumFaces * 3);
		vertexCount += mesh->mNumVertices;
		bonesCount += mesh->mNumBones;

		hasTexCoords = mesh->mNumUVComponents > 0 ? true : hasTexCoords;
	}

	for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
		if (scene->mMaterials[i]->GetTextureCount(aiTextureType_NORMALS) > 0)
			hasTangents = true;
	}

	if (indexCount > 65535) {
		meshData->indices->SetType(COMPONENT_UNSIGNED_INT);
	}
	else {
		meshData->indices->SetType(COMPONENT_UNSIGNED_SHORT);
	}

	meshData->vertices->SetType(COMPONENT_FLOAT);
	meshData->normals->SetType(COMPONENT_PACKED_FLOAT);
	meshData->texCoords->SetType(COMPONENT_HALF_FLOAT);
	meshData->tangents->SetType(COMPONENT_PACKED_FLOAT);

	meshData->SetIndexCount(indexCount);
	meshData->SetVertexCount(vertexCount);

	if (scene->HasAnimations() || bonesCount > 0) {



	}

	uint32_t usedFaces = 0;
	uint32_t usedVertices = 0;
	uint32_t loadedVertices = 0;

	uint32_t* indices = new uint32_t[indexCount];
	float* vertices = new float[vertexCount * 3];
	float* normals = new float[vertexCount * 4];
	float* texCoords = hasTexCoords ? new float[vertexCount * 2] : nullptr;
	float* tangents = hasTangents ? new float[vertexCount * 4] : nullptr;

	for (uint32_t i = 0; i < scene->mNumMaterials; i++) {

		Material* material = LoadMaterial(scene->mMaterials[i], directoryPath);

		SubData* subData = new SubData;
		subData->materialIndex = i;
		subData->indicesOffset = usedFaces * 3;

		for (auto mesh : meshSorted[i]) {
			// Copy vertices
			for (uint32_t j = 0; j < mesh->mNumVertices; j++) {
				vertices[usedVertices * 3] = mesh->mVertices[j].x;
				vertices[usedVertices * 3 + 1] = mesh->mVertices[j].y;
				vertices[usedVertices * 3 + 2] = mesh->mVertices[j].z;

				vec3 normal = normalize(vec3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z));

				normals[usedVertices * 4] = normal.x;
				normals[usedVertices * 4 + 1] = normal.y;
				normals[usedVertices * 4 + 2] = normal.z;
				normals[usedVertices * 4 + 3] = 0.0f;

				if (hasTangents && mesh->mTangents != NULL) {
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

				if (hasTexCoords && mesh->mTextureCoords[0] != NULL) {
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

		subData->numIndices = usedFaces * 3 - subData->indicesOffset;
		meshData->subData.push_back(subData);
		meshData->materials.push_back(material);

	}

	meshData->indices->Set(indices);
	meshData->vertices->Set(vertices);
	meshData->normals->Set(normals);

	if (hasTexCoords)
		meshData->texCoords->Set(texCoords);
	if (hasTangents)
		meshData->tangents->Set(tangents);

	return meshData;

}

Material* ModelLoader::LoadMaterial(aiMaterial* assimpMaterial, string directory) {

	Material* material = new Material();

	aiColor3D diffuse;
	aiColor3D specular;
	aiColor3D ambient;

	assimpMaterial->Get(AI_MATKEY_SHININESS, material->specularHardness);
	assimpMaterial->Get(AI_MATKEY_SHININESS_STRENGTH, material->specularIntensity);
	assimpMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
	assimpMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specular);
	assimpMaterial->Get(AI_MATKEY_COLOR_AMBIENT, ambient);

	material->diffuseColor = vec3(diffuse.r, diffuse.g, diffuse.b);
	material->specularColor = vec3(specular.r, specular.g, specular.b);
	material->ambientColor = vec3(ambient.r, ambient.g, ambient.b);

	if (assimpMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
		aiString aiPath;
		string path;
		assimpMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiPath);
		path = directory + string(aiPath.C_Str());
		material->diffuseMap = new Texture(path.c_str());
	}
	if (assimpMaterial->GetTextureCount(aiTextureType_NORMALS) > 0) {
		aiString aiPath;
		string path;
		assimpMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiPath);
		path = directory + string(aiPath.C_Str());
		material->normalMap = new Texture(path.c_str(), true);
	}

	material->Update();

	return material;

}