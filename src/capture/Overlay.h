#pragma once

#include "Common.h"

#include "renderer/PrimitiveBatch.h"
#include "volume/AABB.h"

#include <array>

namespace Atlas::Capture {

    // Debug lines drawn into the captured images through the engine primitive batches,
    // e.g. bounds of entities, invisible entities like players, trails and query results
    class Overlay {

    public:
        Overlay();

        void Clear();

        void Line(vec3 from, vec3 to, vec3 color);

        void Box(Volume::AABB aabb, vec3 color);

        void Cross(vec3 position, float size, vec3 color);

        // Wireframe cylinder marker, e.g. for players without a mesh
        void Figure(vec3 feet, float height, float radius, vec3 color);

        void Axes(vec3 position, float length);

        std::span<Ref<Renderer::PrimitiveBatch>> GetBatches();

        bool depthTest = false;

    private:
        Ref<Renderer::PrimitiveBatch>& GetBatch();

        std::array<Ref<Renderer::PrimitiveBatch>, 2> batches;

    };

}
