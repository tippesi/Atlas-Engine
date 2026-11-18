#pragma once

#include "../System.h"

#include <vector>
#include <span>
#include <limits>

namespace Atlas {

    namespace Mesh {

        enum class SimplifyVertexLock : uint8_t {
            None = 0,
            // Only collapses onto vertices at the same position, e.g. across a UV seam
            Protect,
            // Never moves
            Lock
        };

        enum class SimplifyRegularization : uint8_t {
            None = 0,
            Light,
            Full
        };

        struct SimplifyOptions {
            // Share of the indices which is kept, a target index count above zero replaces it
            float targetRatio = 0.5f;
            size_t targetIndexCount = 0;

            // Stops before the error gets larger, relative to the extent of the mesh unless absoluteError is set
            float maxError = std::numeric_limits<float>::max();
            bool absoluteError = false;

            // Vertices on open edges stay where they are
            bool lockBorder = false;
            // Collapses across attribute seams where the error stays low
            bool permissive = false;
            // More even triangle sizes and shapes at a small cost in geometric quality, e.g. for simulation meshes
            SimplifyRegularization regularization = SimplifyRegularization::None;

            // Per vertex attributes with attributeCount floats each, weighted against the position error
            std::span<const float> attributes;
            std::span<const float> attributeWeights;
            size_t attributeCount = 0;

            // Per vertex, empty leaves every vertex free
            std::span<const SimplifyVertexLock> vertexLocks;
        };

        struct SimplifyResult {
            std::vector<uint32_t> indices;
            // In the units of maxError
            float error = 0.0f;
        };

        // Simplification of triangle meshes and the remapping and optimization steps around it. The engine uses this
        // class instead of a simplification library directly
        class MeshSimplifier {

        public:
            static SimplifyResult Simplify(std::span<const uint32_t> indices, std::span<const vec3> positions,
                const SimplifyOptions& options);

            // Extent of the mesh which relative errors and attribute weights refer to
            static float GetScale(std::span<const vec3> positions);

            // First vertex at the same position for each vertex
            static std::vector<uint32_t> GeneratePositionRemap(std::span<const vec3> positions);

            // Welds vertices at the same position and drops unused ones. Returns the welded positions and indices,
            // remap gets the welded vertex of each input vertex
            static std::vector<vec3> Weld(std::span<const uint32_t> indices, std::span<const vec3> positions,
                std::vector<uint32_t>& weldedIndices, std::vector<uint32_t>& remap);

            // Normal of each corner (index), corners at an edge sharper than the crease angle (radians) get separate ones
            static std::vector<vec3> GenerateCornerNormals(std::span<const uint32_t> indices, std::span<const vec3> positions,
                float creaseAngle);

            // Orders the triangles for the post transform vertex cache
            static void OptimizeVertexCache(std::vector<uint32_t>& indices, size_t vertexCount);

            // Gives corners with the same vertex and normal one new vertex. Returns the vertex count, remap gets the new
            // vertex of each corner
            static size_t GenerateCornerVertices(std::span<const uint32_t> indices, std::span<const vec3> cornerNormals,
                std::vector<uint32_t>& remap);

            // New vertex of each vertex in the order the indices fetch them, unused ones get ~0u. Returns the used count
            static size_t OptimizeVertexFetchRemap(std::span<const uint32_t> indices, size_t vertexCount,
                std::vector<uint32_t>& remap);

        };

    }

}
