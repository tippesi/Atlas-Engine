#include "RayTracingManager.h"

#include "graphics/GraphicsDevice.h"
#include "graphics/ASBuilder.h"
#include "resource/ResourceManager.h"
#include "mesh/Mesh.h"
#include "terrain/Terrain.h"

namespace Atlas::RayTracing {

    JobGroup RayTracingManager::bvhUpdateGroup{ "Bvh manager update" };
	std::vector<Ref<Graphics::BLAS>> RayTracingManager::blases;
	std::atomic_bool RayTracingManager::meshBVHsRequested = false;
	std::mutex RayTracingManager::meshBuildMutex;

	void RayTracingManager::Update() {

        // This crashes when we start with path tracing and do the bvh build async
        // Launch BVH builds asynchronously
        auto buildRTStructure = [](JobData) {
            // Mesh BVHs are only needed by rt effects, while terrain BVHs are also needed by the terrain LoD
            JobGroup terrainBuildGroup{ "Terrain bvh build" };
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
                    JobSystem::Execute(terrainBuildGroup, [cell = cell, nodeStretch, heightStretch](JobData&) {
                        cell->BuildBVH(nodeStretch, heightStretch);
                        });
                }
            }

            // The streaming can't unload the geometry of meshes while their BVHs are built
            if (meshBVHsRequested.exchange(false)) {
                std::scoped_lock meshLock(meshBuildMutex);
                auto meshes = ResourceManager<Mesh::Mesh>::GetOwnedResources();

                JobGroup meshBuildGroup{ "Mesh bvh build" };
                for (const auto& mesh : meshes) {
                    if (!mesh.IsLoaded() || !mesh->IsGeometryResident())
                        continue;
                    if (mesh->IsBVHBuilt() || !mesh->rayTrace)
                        continue;
                    if (mesh->data.GetIndexCount() == 0 ||
                        mesh->data.GetVertexCount() == 0)
                        continue;

                    JobSystem::Execute(meshBuildGroup, [mesh](JobData&) {
                        mesh->BuildBVH(false);
                        });
                }

                JobSystem::Wait(meshBuildGroup);

                auto device = Graphics::GraphicsDevice::DefaultDevice;
                if (device->support.hardwareRayTracing)
                    BuildStaticBLAS(meshes, terrains);
            }

            JobSystem::Wait(terrainBuildGroup);
            };

        if (bvhUpdateGroup.HasFinished()) {
            JobSystem::Execute(bvhUpdateGroup, buildRTStructure);
        }

	}

    void RayTracingManager::RequestMeshBVHs() {

        meshBVHsRequested = true;

    }

    void RayTracingManager::BuildStaticBLAS(std::vector<ResourceHandle<Mesh::Mesh>>& meshes,
        std::vector<ResourceHandle<Terrain::Terrain>>& terrains) {        

        Graphics::ASBuilder asBuilder;

        // Only static meshes, this runs every frame for all meshes and usually finds none
        std::vector<Mesh::Mesh*> builtMeshes;
        for (auto& mesh : meshes) {
            if (!mesh.IsLoaded() || !mesh->IsBVHBuilt() || !mesh->blas->needsBvhRefresh || mesh->blas->blas->isDynamic)
                continue;

            blases.push_back(mesh->blas->blas);
            builtMeshes.push_back(mesh.Get().get());
        }

        size_t blasBuiltCount = 0;
        if (!blases.empty()) {
            blasBuiltCount = asBuilder.BuildBLAS(blases);
        }

        // Copy the non-compacted versions over
        for (size_t i = 0; i < blasBuiltCount; i++) {
            builtMeshes[i]->blas->blas = blases[i];
            builtMeshes[i]->blas->needsBvhRefresh = false;
        }

        blases.clear();

    }

}
