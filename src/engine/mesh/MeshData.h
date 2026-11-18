#pragma once

#include "../System.h"
#include "../volume/AABB.h"
#include "raytracing/BLAS.h"
#include "resource/Resource.h"
#include "DataComponent.h"
#include "Skeleton.h"
#include "Material.h"
#include "animation/Animation.h"
#include "pipeline/PipelineConfig.h"

#include <vector>

namespace Atlas {

    namespace RayTracing {
        class RayTracingWorld;
    }

    namespace Mesh {

        struct MeshSubDataLod {
            uint32_t indicesOffset;
            uint32_t indicesCount;
        };

        struct MeshLodSettings {
            // Including the full detail level, at most MeshData::maxLodCount
            int32_t maxLodCount = 5;
            // Index count of a level relative to the one before
            float reduction = 0.5f;
            // Levels which keep more of the indices of the level before are dropped and end the chain
            float minReduction = 0.8f;
        };

        struct MeshSubData {

            std::string name;

            uint32_t indicesOffset;
            uint32_t indicesCount;

            // Levels of detail from 1 on, their indices follow the ones of level 0 in the index buffer
            std::vector<MeshSubDataLod> lods;

            ResourceHandle<Material> material;
            int32_t materialIdx;

            Volume::AABB aabb;

            // Mesh units covered by one unit of the texture coordinates, texture streaming derives resolutions from it
            float uvScale = 0.0f;

            // Graphs of the material are compiled in the background, the regular pipelines are used until they are done
            // Renderers read these for every draw, so they are next to the other members draws need
            bool mainGraphPending = false;
            bool shadowGraphPending = false;
            // The configs below are the ones of the graph
            bool mainUsesGraph = false;
            bool shadowUsesGraph = false;

            PipelineConfig mainConfig;
            PipelineConfig shadowConfig;
            // Used for instances with reversed winding
            PipelineConfig mainReversedWindingConfig;
            PipelineConfig shadowReversedWindingConfig;
            // Variants of the configs above for simulated soft bodies, created on their first draw
            PipelineConfig mainSoftBodyConfig;
            PipelineConfig shadowSoftBodyConfig;
            PipelineConfig mainSoftBodyReversedWindingConfig;
            PipelineConfig shadowSoftBodyReversedWindingConfig;

            void ResetPipelines();

            // The indices of a level of detail, level 0 is the sub data itself
            MeshSubDataLod GetLod(int32_t lod) const;
        
        };

        class MeshData {

            friend class Mesh;
            friend class RayTracing::RayTracingWorld;

        public:
            // Has to match MAX_LOD_COUNT in culling/common.hsh
            static constexpr int32_t maxLodCount = 5;

            /**
             *
             */
            MeshData();

            /**
             *
             * @param data
             */
             MeshData(const MeshData& data);

            /**
             *
             * @param that
             * @return
             */
            MeshData& operator=(const MeshData& that);

            /**
             *
             * @param count
             */
            void SetIndexCount(int32_t count);

            /**
             *
             */
            int32_t GetIndexCount() const;

            /**
             *
             * @param count
             */
            void SetVertexCount(int32_t count);

            /**
             *
             * @return
             */
            int32_t GetVertexCount() const;

            /**
             * Builds a blas from the data
             * @param lod The level of detail whose triangles are used
             */
            void BuildBVHData(std::vector<RayTracing::BLAS::Triangle>& triangles, int32_t lod = 0);

            bool IsSkinned() const;

            /**
             * Skins the vertices and normals of a skinned mesh into the rest pose of its skeleton
             */
            void GetRestPoseVertices(std::vector<vec3>& restVertices, std::vector<vec4>& restNormals) const;


            /**
             * Returns the sub data a triangle belongs to, e.g. to get the material of a ray hit
             * @param triangleIndex Index of the triangle like Volume::RayResult::triangleIndex
             * @return The sub data or nullptr for invalid indices
             */
            const MeshSubData* GetSubDataOfTriangle(int32_t triangleIndex) const;

            /**
             * Simplifies the sub data into levels of detail with their own copies of the vertices, replaces existing levels
             */
            void GenerateLods(const MeshLodSettings& settings = {});

            void ClearLods();

            /**
             * Replaces the levels of detail with the ones of generated data, a copy of this data with other levels
             */
            void TakeLods(MeshData& generated);

            /**
             * Updates the texture coordinate scales of the sub data
             */
            void UpdateUVScales();

            // Including level 0
            int32_t GetLodCount() const;

            // Indices of the levels from 1 on, everything else only uses the indices of level 0
            DataComponent<uint32_t> lodIndices;
            // Geometric error of the levels from 1 on in mesh units
            std::vector<float> lodErrors;
            // The levels from 1 on have copies of their vertices at the end of the vertex data, in the order they are
            // fetched
            int32_t lodVertexCount = 0;
            // The settings the levels of detail were generated with
            MeshLodSettings lodSettings;

            std::string name;

            DataComponent<uint32_t> indices;

            DataComponent<vec3> vertices;
            DataComponent<vec2> texCoords;
            // Optional second set, e.g. the element indices of Pivot Painter
            DataComponent<vec2> texCoords1;
            DataComponent<vec4> normals;
            DataComponent<vec4> tangents;
            DataComponent<vec4> colors;
            DataComponent<vec4> joints;
            DataComponent<vec4> weights;

            Skeleton skeleton;

            std::vector<ResourceHandle<Material>> materials;
            // The clips of the skeleton, they load with the mesh
            std::vector<ResourceHandle<Animation::Animation>> animations;
            std::vector<MeshSubData> subData;

            int32_t primitiveType = 0;

            Volume::AABB aabb;
            float radius = 0.0f;

        private:
            void DeepCopy(const MeshData& that);

            int32_t indexCount = 0;
            int32_t vertexCount = 0;

        };

    }

}