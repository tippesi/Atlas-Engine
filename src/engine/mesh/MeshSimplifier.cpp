#include "MeshSimplifier.h"

#include <meshoptimizer.h>

namespace Atlas {

    namespace Mesh {

        SimplifyResult MeshSimplifier::Simplify(std::span<const uint32_t> indices, std::span<const vec3> positions,
            const SimplifyOptions& options) {

            AE_ASSERT(options.attributes.empty() || options.attributes.size() == positions.size() * options.attributeCount);
            AE_ASSERT(options.attributeWeights.size() == (options.attributes.empty() ? 0 : options.attributeCount));
            AE_ASSERT(options.vertexLocks.empty() || options.vertexLocks.size() == positions.size());

            auto targetCount = options.targetIndexCount > 0 ? options.targetIndexCount :
                size_t(float(indices.size()) * options.targetRatio);
            targetCount = targetCount / 3 * 3;

            uint32_t flags = 0;
            if (options.absoluteError)
                flags |= meshopt_SimplifyErrorAbsolute;
            if (options.lockBorder)
                flags |= meshopt_SimplifyLockBorder;
            if (options.permissive)
                flags |= meshopt_SimplifyPermissive;
            if (options.regularization == SimplifyRegularization::Light)
                flags |= meshopt_SimplifyRegularizeLight;
            else if (options.regularization == SimplifyRegularization::Full)
                flags |= meshopt_SimplifyRegularize;

            std::vector<unsigned char> locks;
            if (!options.vertexLocks.empty()) {
                locks.resize(positions.size());
                for (size_t i = 0; i < positions.size(); i++) {
                    switch (options.vertexLocks[i]) {
                    case SimplifyVertexLock::Protect: locks[i] = meshopt_SimplifyVertex_Protect; break;
                    case SimplifyVertexLock::Lock: locks[i] = meshopt_SimplifyVertex_Lock; break;
                    default: locks[i] = 0; break;
                    }
                }
            }

            SimplifyResult result;
            result.indices.resize(indices.size());
            auto count = meshopt_simplifyWithAttributes(result.indices.data(), indices.data(), indices.size(),
                reinterpret_cast<const float*>(positions.data()), positions.size(), sizeof(vec3),
                options.attributes.empty() ? nullptr : options.attributes.data(), options.attributeCount * sizeof(float),
                options.attributeWeights.empty() ? nullptr : options.attributeWeights.data(),
                options.attributes.empty() ? 0 : options.attributeCount, locks.empty() ? nullptr : locks.data(),
                targetCount, options.maxError, flags, &result.error);
            result.indices.resize(count);

            return result;

        }

        float MeshSimplifier::GetScale(std::span<const vec3> positions) {

            return meshopt_simplifyScale(reinterpret_cast<const float*>(positions.data()), positions.size(), sizeof(vec3));

        }

        std::vector<uint32_t> MeshSimplifier::GeneratePositionRemap(std::span<const vec3> positions) {

            std::vector<uint32_t> remap(positions.size());
            meshopt_generatePositionRemap(remap.data(), reinterpret_cast<const float*>(positions.data()), positions.size(), sizeof(vec3));
            return remap;

        }

        std::vector<vec3> MeshSimplifier::Weld(std::span<const uint32_t> indices, std::span<const vec3> positions,
            std::vector<uint32_t>& weldedIndices, std::vector<uint32_t>& remap) {

            auto positionRemap = GeneratePositionRemap(positions);
            weldedIndices.resize(indices.size());
            for (size_t i = 0; i < indices.size(); i++)
                weldedIndices[i] = positionRemap[indices[i]];

            std::vector<uint32_t> compaction;
            auto count = OptimizeVertexFetchRemap(weldedIndices, positions.size(), compaction);

            std::vector<vec3> welded(count);
            remap.assign(positions.size(), ~0u);
            for (size_t i = 0; i < positions.size(); i++) {
                auto target = compaction[positionRemap[i]];
                if (target == ~0u)
                    continue;
                welded[target] = positions[i];
                remap[i] = target;
            }
            for (auto& index : weldedIndices)
                index = compaction[index];

            return welded;

        }

        std::vector<vec3> MeshSimplifier::GenerateCornerNormals(std::span<const uint32_t> indices,
            std::span<const vec3> positions, float creaseAngle) {

            std::vector<vec3> normals(indices.size());
            meshopt_generateNormals(reinterpret_cast<float*>(normals.data()), indices.data(), indices.size(), reinterpret_cast<const float*>(positions.data()),
                positions.size(), sizeof(vec3), creaseAngle, 0.0f);
            return normals;

        }

        void MeshSimplifier::OptimizeVertexCache(std::vector<uint32_t>& indices, size_t vertexCount) {

            meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), vertexCount);

        }

        size_t MeshSimplifier::GenerateCornerVertices(std::span<const uint32_t> indices, std::span<const vec3> cornerNormals,
            std::vector<uint32_t>& remap) {

            AE_ASSERT(cornerNormals.size() == indices.size());

            const meshopt_Stream streams[] = {
                { indices.data(), sizeof(uint32_t), sizeof(uint32_t) },
                { cornerNormals.data(), sizeof(vec3), sizeof(vec3) }
            };
            remap.resize(indices.size());
            return meshopt_generateVertexRemapMulti(remap.data(), nullptr, indices.size(), indices.size(), streams, 2);

        }

        size_t MeshSimplifier::OptimizeVertexFetchRemap(std::span<const uint32_t> indices, size_t vertexCount,
            std::vector<uint32_t>& remap) {

            remap.resize(vertexCount);
            return meshopt_optimizeVertexFetchRemap(remap.data(), indices.data(), indices.size(), vertexCount);

        }

    }

}
