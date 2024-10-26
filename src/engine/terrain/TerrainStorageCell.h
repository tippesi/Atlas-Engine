#pragma once

#include "../System.h"
#include "../Material.h"
#include "../physics/ShapesManager.h"
#include "../raytracing/BLAS.h"
#include "../volume/AABB.h"

#include <vector>

namespace Atlas {

    namespace Terrain {

        class TerrainStorage;

        /**
         * Stores the material information for a terrain node.
         * Only LoD0 uses a splatmap and the materials
         */
        class TerrainStorageCell {

        public:
            TerrainStorageCell(TerrainStorage* storage);

            bool IsLoaded();

            void BuildBVH();

            int32_t x = 0;
            int32_t y = 0;
            int32_t LoD = 0;

            vec2 position;

            Volume::AABB aabb;
            mat3x4 inverseMatrix;

            Physics::ShapeRef shape;
            Ref<RayTracing::BLAS> blas;

            std::vector<float> heightData;
            std::vector<uint8_t> materialIdxData;

            Texture::Texture2D heightField;
            Texture::Texture2D normalMap;
            Texture::Texture2D splatMap;

            TerrainStorage* storage = nullptr;

        };

    }

}