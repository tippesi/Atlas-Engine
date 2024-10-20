#include "RayTracingManager.h"

#include "graphics/GraphicsDevice.h"
#include "graphics/ASBuilder.h"
#include "resource/ResourceManager.h"
#include "mesh/Mesh.h"

namespace Atlas::RayTracing {

	JobGroup RayTracingManager::bvhUpdateGroup;
	std::vector<Ref<Graphics::BLAS>> RayTracingManager::blases;

	void RayTracingManager::Update() {

#ifdef AE_BINDLESS
        // This crashes when we start with path tracing and do the bvh build async
        // Launch BVH builds asynchronously
        auto buildRTStructure = [&](JobData) {
            auto sceneMeshes = ResourceManager<Mesh::Mesh>::GetResources();

            JobGroup bvhBuildGroup;
            for (const auto& mesh : sceneMeshes) {
                if (!mesh.IsLoaded())
                    continue;
                if (mesh->IsBVHBuilt() || !mesh->rayTrace)
                    continue;
                if (mesh->data.GetIndexCount() == 0 ||
                    mesh->data.GetVertexCount() == 0)
                    continue;

                JobSystem::Execute(bvhBuildGroup, [mesh](JobData&) {
                    mesh->BuildBVH(false);
                    });
            }

            JobSystem::Wait(bvhBuildGroup);

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            if (device->support.hardwareRayTracing)
                BuildStaticBLAS(sceneMeshes);
            };

        if (bvhUpdateGroup.HasFinished()) {
            JobSystem::Execute(bvhUpdateGroup, buildRTStructure);
        }
#endif

	}

    void RayTracingManager::BuildStaticBLAS(std::vector<ResourceHandle<Mesh::Mesh>>& meshes) {        

        Graphics::ASBuilder asBuilder;

        for (auto it = meshes.begin(); it != meshes.end();) {
            auto& mesh = *it;

            // Only want static meshes
            if (!mesh.IsLoaded() || !mesh->IsBVHBuilt() || !mesh->needsBvhRefresh || mesh->blas->isDynamic) {
                it = meshes.erase(it);
            }
            else {
                blases.push_back(mesh->blas);
                ++it;
            }
        }

        size_t blasBuiltCount = 0;
        if (!blases.empty()) {
            blasBuiltCount = asBuilder.BuildBLAS(blases);
        }

        // Copy the non-compacted versions over
        for (size_t i = 0; i < blasBuiltCount; i++) {
            meshes[i]->blas = blases[i];
            meshes[i]->needsBvhRefresh = false;
        }

        blases.clear();

    }

}