#include "MeshStreaming.h"
#include "StreamingManager.h"
#include "../raytracing/RayTracingManager.h"

#include <algorithm>
#include <limits>

namespace Atlas::Streaming {

    // Level 0 has the geometry resident, level 1 nothing
    class MeshStreaming::Entry : public ResidencyEntry {

    public:
        bool Prepare() override {

            auto resident = mesh.lock();
            if (!resident)
                return false;

            // Shadows and ray tracing count as well, impostors replace the geometry beyond their distances
            const auto& settings = StreamingManager::settings.meshes;
            hasImpostor = resident->impostor.IsLoaded() && resident->impostor->isGenerated;
            auto cullingDistance = std::max(resident->distanceCulling,
                resident->castShadow ? resident->shadowDistanceCulling : 0.0f);
            auto geometryDistance = hasImpostor ? std::max(resident->impostorDistance,
                resident->castShadow ? resident->impostorShadowDistance : 0.0f) : cullingDistance;
            if (rayTracing && resident->rayTrace)
                geometryDistance = std::max(geometryDistance,
                    std::min(resident->rayTraceDistanceCulling, settings.rayTracingDistance));

            // Over the budget meshes with impostors are shown as impostors only, the farthest first. A mesh keeps its
            // priority and whether it's needed until its next request
            budgetLevel = hasImpostor ? 1 : 0;
            desiredLevel = distance < geometryDistance + settings.distanceMargin ? 0 : 1;
            if (requested)
                needed = distance < cullingDistance + settings.distanceMargin;
            if (distance < std::numeric_limits<float>::max())
                priority = geometryDistance > 0.0f ? -distance / geometryDistance : -distance;

            distance = std::numeric_limits<float>::max();
            rayTracing = false;
            requested = false;
            return true;

        }

        size_t GetSize(int32_t level) const override {

            return level == 0 ? geometrySize : 0;

        }

        bool Load(int32_t level) override {

            if (auto resident = mesh.lock())
                resident->CreateBuffers();
            return true;

        }

        void Apply() override {

            if (auto resident = mesh.lock()) {
                resident->geometryResident = true;
                geometrySize = resident->GetGeometrySize();
            }

        }

        Release ReleaseTo(int32_t level) override {

            // Ray tracing might build the BVH of the mesh right now. Meshes which are still needed keep their
            // geometry until the impostor that replaces it is resident
            auto resident = mesh.lock();
            if (!resident || !canUnload || (needed && hasImpostor && !resident->impostor->IsResident()))
                return Release::Wait;

            resident->UnloadGeometry();
            return Release::Done;

        }

        std::weak_ptr<Mesh::Mesh> mesh;
        size_t geometrySize = 0;

        bool requested = false;
        float distance = std::numeric_limits<float>::max();
        bool rayTracing = false;

        bool hasImpostor = false;
        // An instance is within the culling distances
        bool needed = true;

        static bool canUnload;

    };

    bool MeshStreaming::Entry::canUnload = false;
    Residency MeshStreaming::residency{ "Mesh streaming", 2 };

    void MeshStreaming::Request(std::span<const MeshRequest> requests, bool rayTracing) {

        std::scoped_lock lock(residency.mutex);

        for (const auto& [handle, distance] : requests) {
            const auto& mesh = *handle;
            if (!mesh.IsLoaded() || mesh->data.IsSkinned())
                continue;

            auto entry = static_cast<Entry*>(residency.Find(mesh.Get().get()));
            if (!entry) {
                auto newEntry = CreateRef<Entry>();
                newEntry->mesh = mesh.Get();
                newEntry->geometrySize = mesh->GetGeometrySize();
                newEntry->residentLevel = mesh->IsGeometryResident() ? 0 : 1;
                residency.Insert(mesh.Get().get(), newEntry);
                entry = newEntry.get();
            }

            entry->requested = true;
            entry->distance = std::min(entry->distance, distance);
            entry->rayTracing |= rayTracing;
        }

    }

    void MeshStreaming::LoadGeometry(const ResourceHandle<Mesh::Mesh>& mesh) {

        if (!mesh->IsGeometryResident())
            residency.LoadNow(mesh.Get().get(), 0);

    }

    void MeshStreaming::Update(const StreamingSettings::Meshes& settings) {

        // The geometry can't be unloaded while ray tracing builds the BVHs of meshes
        std::unique_lock rayTracingLock(RayTracing::RayTracingManager::meshBuildMutex, std::try_to_lock);
        Entry::canUnload = rayTracingLock.owns_lock();

        residency.Update(settings);

    }

}
