#include "RayTracingManager.h"

#include "graphics/GraphicsDevice.h"
#include "resource/ResourceManager.h"
#include "mesh/Mesh.h"

namespace Atlas::RayTracing {

	JobGroup RayTracingManager::bvhUpdateGroup;

	void RayTracingManager::Update() {

#ifdef AE_BINDLESS
        // This crashes when we start with path tracing and do the bvh build async
        // Launch BVH builds asynchronously
        auto buildRTStructure = [&](JobData) {
            auto sceneMeshes = ResourceManager<Mesh::Mesh>::GetResources();

            for (const auto& mesh : sceneMeshes) {
                if (!mesh.IsLoaded())
                    continue;
                if (mesh->IsBVHBuilt() || !mesh->rayTrace)
                    continue;
                if (mesh->data.GetIndexCount() == 0 ||
                    mesh->data.GetVertexCount() == 0)
                    continue;
                JobSystem::Execute(bvhUpdateGroup, [mesh](JobData&) {
                    
                    mesh->BuildBVH(false);
                    });
            }
            };

        if (bvhUpdateGroup.HasFinished()) {
            JobSystem::Execute(bvhUpdateGroup, buildRTStructure);
        }
#endif

	}

}