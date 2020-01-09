#ifndef AE_RESOURCEMANAGER_H
#define AE_RESOURCEMANAGER_H

#include "System.h"

#include "mesh/Mesh.h"
#include "audio/AudioData.h"
#include "terrain/Terrain.h"
#include "Material.h"

#include <mutex>

namespace Atlas {

	class ResourceManager {

	public:
		static Mesh::Mesh* GetMesh(std::string path);
		static Material* GetMaterial(std::string path);
		static Audio::AudioData* GetAudio(std::string path);
		static Terrain::Terrain* GetTerrain(std::string path);

		static void AddMesh(Mesh::Mesh* mesh);
		static void AddMaterial(Material* material);
		static void AddAudio(Audio::AudioData* audio);
		static void AddTerrain(Terrain::Terrain* terrain);

		static void RemoveMesh(Mesh::Mesh* mesh);
		static void RemoveMaterial(Material* material);
		static void RemoveAudio(Audio::AudioData* audio);
		static void RemoveTerrain(Terrain::Terrain* terrain);

		static std::vector<Mesh::Mesh*> GetMeshes();
		static std::vector<Material*> GetMaterials();
		static std::vector<Audio::AudioData*> GetAudios();
		static std::vector<Terrain::Terrain*> GetTerrains();

	private:
		static std::vector<Mesh::Mesh*> meshes;
		static std::vector<Material*> materials;
		static std::vector<Audio::AudioData*> audios;
		static std::vector<Terrain::Terrain*> terrains;

		static std::mutex meshMutex;
		static std::mutex materialMutex;
		static std::mutex audioMutex;
		static std::mutex terrainMutex;

	};

}

#endif