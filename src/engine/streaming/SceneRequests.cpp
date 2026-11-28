#include "SceneRequests.h"
#include "StreamingManager.h"
#include "../scene/Scene.h"
#include "../profiler/CPUProfiler.h"

namespace Atlas::Streaming {

    void SceneRequests::Request(Scene::Scene* scene) {

        CPUProfiler::Scope profilerScope("Request streamed resources");

        auto& gpuScene = scene->renderState.gpuScene;

        // The last distances are requested again until the GPU delivers new ones, without new ones (e.g. while the
        // scene isn't rendered) nothing is unloaded
        gpuScene.TakeMeshDistances(meshDistances);
        auto focalLength = gpuScene.GetFocalLength();
        if (focalLength <= 0.0f)
            return;

        // Materials are shared by many meshes, each one is requested once with its largest resolution
        materialResolutions.clear();
        auto requestMaterials = [&](const ResourceHandle<Mesh::Mesh>& mesh, float distance) {
            for (const auto& subData : mesh->data.subData) {
                if (!subData.material.IsLoaded())
                    continue;

                auto material = subData.material.Get().get();
                auto resolution = TextureStreaming::GetResolution(subData.uvScale / material->tiling,
                    distance, focalLength);
                auto& materialResolution = materialResolutions[material];
                materialResolution = glm::max(materialResolution, resolution);
            }
            };

        // Each update requests a part of the meshes in rotation, reductions wait for many updates anyway
        meshRequests.clear();
        impostorRequests.clear();
        const auto& meshSettings = StreamingManager::settings.meshes;
        const auto& slots = gpuScene.slots;
        auto slotCount = glm::min(meshDistances.meshIds.size(), slots.size());
        const size_t requestRotation = Residency::requestRotation;
        requestOffset = (requestOffset + 1) % requestRotation;
        for (size_t i = requestOffset; i < slotCount; i += requestRotation) {
            const auto& slot = slots[i];
            if (meshDistances.meshIds[i] == 0 || slot.mesh.GetID() != meshDistances.meshIds[i] || !slot.mesh.IsLoaded())
                continue;

            // Meshes without an instance that needs them keep the largest distance, a NaN. They are requested
            // anyway, so their impostors are unloaded as well
            auto positionDistance = meshDistances.positionDistances[i];
            meshRequests.push_back({ std::addressof(slot.mesh),
                std::isfinite(positionDistance) ? positionDistance : std::numeric_limits<float>::max() });

            auto distance = meshDistances.distances[i];
            if (std::isfinite(distance))
                requestMaterials(slot.mesh, distance);

            // Impostors are drawn beyond their distances, meshes without a needed instance don't request theirs
            const auto& mesh = slot.mesh;
            if (std::isfinite(positionDistance) && mesh->impostor.IsLoaded() && mesh->impostor->isGenerated) {
                auto cullingDistance = glm::max(mesh->distanceCulling, mesh->castShadow ? mesh->shadowDistanceCulling : 0.0f);
                auto impostorDistance = mesh->castShadow ? glm::min(mesh->impostorDistance, mesh->impostorShadowDistance) :
                    mesh->impostorDistance;
                if (positionDistance < cullingDistance + meshSettings.distanceMargin) {
                    auto resolution = ImpostorStreaming::GetResolution(mesh->impostor->radius,
                        glm::max(positionDistance, impostorDistance), focalLength);
                    impostorRequests.push_back({ mesh->impostor.Get().get(), resolution });
                }
            }
        }

        // Clutter is drawn around the camera and isn't culled on the GPU, it needs its meshes in full detail
        std::vector<ResourceHandle<Mesh::Mesh>> clutterMeshes;
        if (scene->clutter)
            clutterMeshes = scene->clutter->GetMeshes();
        for (const auto& mesh : clutterMeshes) {
            if (!mesh.IsLoaded())
                continue;
            meshRequests.push_back({ std::addressof(mesh), 0.0f });
            requestMaterials(mesh, 0.0f);
        }

        // Skinned meshes aren't culled on the GPU
        if (scene->HasMainCamera()) {
            auto cameraLocation = scene->GetMainCamera().GetLocation();
            auto& transformComponentPool = scene->entityManager.GetPool<TransformComponent>();
            for (const auto& slot : slots) {
                if (!slot.instanceVertices || !slot.mesh.IsLoaded())
                    continue;

                auto minDistance = std::numeric_limits<float>::max();
                for (auto entity : slot.entities) {
                    // The slots are cleaned up later in the update, entities might be gone since the last one
                    auto transformComponent = transformComponentPool.TryGet(entity);
                    if (!transformComponent)
                        continue;

                    const auto& matrix = transformComponent->globalMatrix;
                    auto scale = glm::max(glm::length(vec3(matrix[0])),
                        glm::max(glm::length(vec3(matrix[1])), glm::length(vec3(matrix[2]))));
                    auto distance = slot.mesh->data.aabb.Transform(matrix).GetDistance(cameraLocation);
                    minDistance = glm::min(minDistance, distance / scale);
                }
                requestMaterials(slot.mesh, minDistance);
            }

            // The terrain samples its materials at the position times their tiling, the distance is the height
            // of the camera above it
            if (scene->terrain.IsLoaded() && scene->terrain->storage) {
                const auto& terrain = scene->terrain;
                auto height = terrain->GetHeight(cameraLocation.x, cameraLocation.z);
                auto bounds = Volume::AABB(terrain->translation,
                    terrain->translation + vec3(terrain->sideLength, terrain->heightScale, terrain->sideLength));
                auto distance = height != Terrain::Terrain::invalidHeight ?
                    glm::abs(cameraLocation.y - height) : bounds.GetDistance(cameraLocation);

                for (const auto& material : terrain->storage->materials) {
                    if (!material.IsLoaded())
                        continue;
                    auto resolution = TextureStreaming::GetResolution(1.0f / material->tiling,
                        distance, focalLength);
                    auto& materialResolution = materialResolutions[material.Get().get()];
                    materialResolution = glm::max(materialResolution, resolution);
                }
            }
        }

        materialRequests.clear();
        for (const auto& [material, resolution] : materialResolutions)
            materialRequests.push_back({ material, resolution });

        MeshStreaming::Request(meshRequests, scene->IsRtDataNeeded());
        TextureStreaming::Request(materialRequests);
        ImpostorStreaming::Request(impostorRequests);

    }

}
