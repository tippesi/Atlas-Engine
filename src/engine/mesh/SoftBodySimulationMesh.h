#pragma once

#include "../System.h"
#include "../volume/AABB.h"

#include <vector>

namespace Atlas {

    namespace Mesh {

        class MeshData;

        struct SoftBodyMeshSettings {
            // Most vertices of the simulation mesh, zero simulates all welded vertices of level 0
            int32_t maxSimulatedVertices = 0;
            // Keeps every vertex on open edges, e.g. the outline of a banner, when the mesh is simplified. Otherwise they
            // only slide along the edges
            bool keepBorders = false;
        };

        // The mesh a soft body simulates: level 0 welded by position and simplified, each render vertex is bound to a
        // simulated triangle
        class SoftBodySimulationMesh {

        public:
            struct Binding {
                uint32_t vertices[3];
                vec3 weights;
                // Distance along the interpolated normal
                float offset;
                // Back faces modelled as their own triangles point the other way than the simulated ones
                float normalSign = 1.0f;
            };

            SoftBodySimulationMesh(const MeshData& data, const SoftBodyMeshSettings& settings);

            // Simulated vertices within any of the bounds (mesh space), bounds without one pin the closest vertex
            std::vector<uint32_t> GetPinnedVertices(const std::vector<Volume::AABB>& pinBounds) const;

            // Simulated vertices within the bounds (mesh space)
            size_t GetVertexCount(const Volume::AABB& bounds) const;

            // Normalized vertex normals of the simulated positions, wound like the simulated triangles
            void CalculateNormals(const std::vector<vec3>& simulatedPositions, std::vector<vec3>& simulatedNormals) const;

            // Position and normal of a render vertex, the normal points like the ones of the render mesh
            void GetRenderVertex(size_t idx, const std::vector<vec3>& simulatedPositions,
                const std::vector<vec3>& simulatedNormals, vec3& position, vec3& normal) const;

            // Rest positions in mesh space
            std::vector<vec3> positions;
            std::vector<uint32_t> indices;
            // Share of the mass of each simulated vertex, a third of the area of its triangles
            std::vector<float> masses;

            // One per render vertex of level 0
            std::vector<Binding> bindings;

        };

    }

}
