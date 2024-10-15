#pragma once

#include "../System.h"
#include "jobsystem/JobGroup.h"
#include "graphics/BLAS.h"
#include "mesh/Mesh.h"

namespace Atlas::RayTracing {

	class RayTracingManager {
		
	public:
		static void Update();

	private:
		static void BuildStaticBLAS(std::vector<ResourceHandle<Mesh::Mesh>>& meshes);

		static JobGroup bvhUpdateGroup;
		static std::vector<Ref<Graphics::BLAS>> blases;

	};

}