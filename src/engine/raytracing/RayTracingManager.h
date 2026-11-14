#pragma once

#include "../System.h"
#include "jobsystem/JobGroup.h"
#include "graphics/BLAS.h"

#include "terrain/Terrain.h"
#include "mesh/Mesh.h"

#include <atomic>
#include <mutex>

namespace Atlas::RayTracing {

	class RayTracingManager {
		
	public:
		static void Update();

		static void RequestMeshBVHs();

		// Held while the BVHs of meshes are built, their geometry can't be unloaded meanwhile
		static std::mutex meshBuildMutex;

	private:
		static void BuildStaticBLAS(std::vector<ResourceHandle<Mesh::Mesh>>& meshes,
			std::vector<ResourceHandle<Terrain::Terrain>>& terrains);

		static JobGroup bvhUpdateGroup;
		static std::vector<Ref<Graphics::BLAS>> blases;
		static std::atomic_bool meshBVHsRequested;

	};

}