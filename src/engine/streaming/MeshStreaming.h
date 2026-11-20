#pragma once

#include "../System.h"
#include "Residency.h"
#include "StreamingSettings.h"
#include "../mesh/Mesh.h"
#include "../resource/Resource.h"

#include <span>

namespace Atlas::Streaming {

    /**
     * Keeps the geometry (GPU buffers and acceleration structures) of requested meshes resident where their instances
     * need it. Meshes far from the camera are drawn with their impostors only. The data of the meshes stays in RAM,
     * physics and tools keep working with it.
     */
    class MeshStreaming {

    public:
        struct MeshRequest {
            const ResourceHandle<Mesh::Mesh>* mesh;
            // The distance of the closest instance to its position
            float distance;
        };

        /**
         * Requests the meshes until the next update, the closest request counts. Whether the geometry is needed
         * follows from the distances of the mesh. Skinned meshes aren't streamed.
         * @param rayTracing Ray tracing needs the geometry up to the ray tracing distance of the meshes
         */
        static void Request(std::span<const MeshRequest> requests, bool rayTracing);

        // Makes the geometry of the mesh resident right away (e.g. for baking), has to run while nothing renders
        static void LoadGeometry(const ResourceHandle<Mesh::Mesh>& mesh);

        // Called by the streaming manager
        static void Update(const StreamingSettings::Meshes& settings);

        static Residency residency;

    private:
        class Entry;

    };

}
