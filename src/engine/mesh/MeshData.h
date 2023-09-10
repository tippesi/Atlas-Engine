#pragma once

#include "../System.h"
#include "../volume/AABB.h"
#include "../scene/RTStructures.h"
#include "DataComponent.h"
#include "Material.h"

#include <vector>

namespace Atlas {

    namespace Scene {
        class RTData;
    }

    namespace Mesh {

        struct MeshSubData {

            std::string name;

            uint32_t indicesOffset;
            uint32_t indicesCount;
            
            Ref<Material> material;
            int32_t materialIdx;

            Volume::AABB aabb;
        
        };

        class MeshData {

            friend class Scene::RTData;

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
             * Applies a transformation matrix to the data.
             * @param transform
             */
            void SetTransform(mat4 transform);

            /**
             * Builds a blas from the data
             */
            void BuildBVH();

            std::string filename;

            DataComponent<uint32_t> indices;

            DataComponent<vec3> vertices;
            DataComponent<vec2> texCoords;
            DataComponent<vec4> normals;
            DataComponent<vec4> tangents;
            DataComponent<vec4> colors;

            std::vector<Ref<Material>> materials;
            std::vector<MeshSubData> subData;

            int32_t primitiveType = 0;

            Volume::AABB aabb;

            mat4 transform;

        private:
            void DeepCopy(const MeshData& that);

            int32_t indexCount = 0;
            int32_t vertexCount = 0;

            std::vector<GPUTriangle> gpuTriangles;
            std::vector<BVHTriangle> gpuBvhTriangles;
            std::vector<GPUBVHNode> gpuBvhNodes;

        };

    }

}