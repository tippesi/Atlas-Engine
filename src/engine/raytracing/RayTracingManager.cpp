#include "RayTracingManager.h"

#include "graphics/GraphicsDevice.h"
#include "graphics/ASBuilder.h"
#include "resource/ResourceManager.h"
#include "mesh/Mesh.h"
#include "terrain/Terrain.h"

namespace Atlas::RayTracing {

	JobGroup RayTracingManager::bvhUpdateGroup;
	std::vector<Ref<Graphics::BLAS>> RayTracingManager::blases;

	void RayTracingManager::Update() {

#ifdef AE_BINDLESS
        // This crashes when we start with path tracing and do the bvh build async
        // Launch BVH builds asynchronously
        auto buildRTStructure = [&](JobData) {
            auto meshes = ResourceManager<Mesh::Mesh>::GetOwnedResources();

            JobGroup bvhBuildGroup;
            for (const auto& mesh : meshes) {
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

            auto terrains = ResourceManager<Terrain::Terrain>::GetOwnedResources();
            for (const auto& terrain : terrains) {
                if (!terrain.IsLoaded())
                    continue;

                auto& storage = terrain->storage;
                auto cells = storage->GetRequestedBvhCellsQueue();

                for (auto cell : cells) {
                    if (!cell->IsLoaded())
                        continue;
                    if (cell->blas && cell->blas->IsBuilt())
                        continue;

                    float nodeStretch = terrain->resolution * powf(2.0f,
                        (float)(terrain->LoDCount - cell->LoD) - 1.0f);
                    float heightStretch = terrain->heightScale;
                    JobSystem::Execute(bvhBuildGroup, [cell = cell, nodeStretch, heightStretch](JobData&) {
                        cell->BuildBVH(nodeStretch, heightStretch);
                        });
                }
            }

            JobSystem::Wait(bvhBuildGroup);

            auto device = Graphics::GraphicsDevice::DefaultDevice;
            if (device->support.hardwareRayTracing)
                BuildStaticBLAS(meshes, terrains);
            };

        if (bvhUpdateGroup.HasFinished()) {
            JobSystem::Execute(bvhUpdateGroup, buildRTStructure);
        }
#endif

	}

    void RayTracingManager::BuildStaticBLAS(std::vector<ResourceHandle<Mesh::Mesh>>& meshes,
        std::vector<ResourceHandle<Terrain::Terrain>>& terrains) {        

        Graphics::ASBuilder asBuilder;

        for (auto it = meshes.begin(); it != meshes.end();) {
            auto& mesh = *it;

            // Only want static meshes
            if (!mesh.IsLoaded() || !mesh->IsBVHBuilt() || !mesh->blas->needsBvhRefresh || mesh->blas->blas->isDynamic) {
                it = meshes.erase(it);
            }
            else {
                blases.push_back(mesh->blas->blas);
                ++it;
            }
        }

        size_t blasBuiltCount = 0;
        if (!blases.empty()) {
            blasBuiltCount = asBuilder.BuildBLAS(blases);
        }

        // Copy the non-compacted versions over
        for (size_t i = 0; i < blasBuiltCount; i++) {
            meshes[i]->blas->blas = blases[i];
            meshes[i]->blas->needsBvhRefresh = false;
        }

        blases.clear();

    }

}