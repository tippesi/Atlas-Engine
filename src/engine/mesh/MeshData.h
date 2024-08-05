#pragma once

#include "../System.h"
#include "../volume/AABB.h"
#include "../buffer/VertexArray.h"
#include "raytracing/RTStructures.h"
#include "DataComponent.h"
#include "Material.h"

#include <vector>

namespace Atlas {

    namespace RayTracing {
        class RayTracingWorld;
    }

    namespace Mesh {

        typedef uint32_t MeshDataUsage;

        typedef enum MeshDataUsageBits {
            MultiBufferedBit = (1 << 0),
            HostAccessBit = (1 << 1),
        } MeshDataUsageBits;

        struct MeshSubData {

            std::string name;

            uint32_t indicesOffset;
            uint32_t indicesCount;
            
            Ref<Material> material;
            int32_t materialIdx;

            Volume::AABB aabb;
        
        };

        class MeshData {

            friend class Mesh;
            friend class RayTracing::RayTracingWorld;

        public:
            /**
             *
             */
            MeshData(MeshDataUsage usage = 0);

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
             * Applies a transformation matrix to the data.
             * @param transform
             */
            void SetTransform(mat4 transform);

             /**
             * Fully updates the buffer data with data available through the MeshData member
             */
            void UpdateData();

            /**
             * Updates the vertex array based on the state of the vertex buffers.
             * @note This is useful when running your own data pipeline
             */
            void UpdateVertexArray();

            std::string filename;

            DataComponent<uint32_t> indices;

            DataComponent<vec3> vertices;
            DataComponent<vec2> texCoords;
            DataComponent<vec4> normals;
            DataComponent<vec4> tangents;
            DataComponent<vec4> colors;

            std::vector<Ref<Material>> materials;
            std::vector<MeshSubData> subData;

            Buffer::VertexArray vertexArray;

            Buffer::IndexBuffer indexBuffer;
            Buffer::VertexBuffer vertexBuffer;
            Buffer::VertexBuffer normalBuffer;
            Buffer::VertexBuffer texCoordBuffer;
            Buffer::VertexBuffer tangentBuffer;
            Buffer::VertexBuffer colorBuffer;

            MeshDataUsage usage = 0;

            int32_t primitiveType = 0;

            Volume::AABB aabb;
            float radius = 0.0f;

            mat4 transform;

        private:
            void DeepCopy(const MeshData& that);

            int32_t indexCount = 0;
            int32_t vertexCount = 0;

        };

    }

}