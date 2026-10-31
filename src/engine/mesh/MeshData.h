#pragma once

#include "../System.h"
#include "../volume/AABB.h"
#include "raytracing/BLAS.h"
#include "resource/Resource.h"
#include "DataComponent.h"
#include "Skeleton.h"
#include "Material.h"
#include "pipeline/PipelineConfig.h"

#include <vector>

namespace Atlas {

    namespace RayTracing {
        class RayTracingWorld;
    }

    namespace Mesh {

        struct MeshSubData {

            std::string name;

            uint32_t indicesOffset;
            uint32_t indicesCount;
            
            ResourceHandle<Material> material;
            int32_t materialIdx;

            Volume::AABB aabb;

            PipelineConfig mainConfig;
            PipelineConfig shadowConfig;
            // Used for instances with reversed winding
            PipelineConfig mainReversedWindingConfig;
            PipelineConfig shadowReversedWindingConfig;
        
        };

        class MeshData {

            friend class Mesh;
            friend class RayTracing::RayTracingWorld;

        public:
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
             */
            void BuildBVHData(std::vector<RayTracing::BLAS::Triangle>& triangles);

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

            std::string name;

            DataComponent<uint32_t> indices;

            DataComponent<vec3> vertices;
            DataComponent<vec2> texCoords;
            DataComponent<vec4> normals;
            DataComponent<vec4> tangents;
            DataComponent<vec4> colors;
            DataComponent<vec4> joints;
            DataComponent<vec4> weights;

            Skeleton skeleton;

            std::vector<ResourceHandle<Material>> materials;
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