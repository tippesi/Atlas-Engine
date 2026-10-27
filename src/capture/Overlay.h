#pragma once

#include "Common.h"

#include "renderer/PrimitiveBatch.h"

#include <array>

namespace Atlas::Capture {

    // Debug lines drawn into the captured images through the engine primitive batches,
    // e.g. bounds of entities, invisible entities like players, trails and query results
    class Overlay {

    public:
        Overlay();

        void Clear();

        // The batch for new lines, depending on depthTest
        Ref<Renderer::PrimitiveBatch>& GetBatch();

        std::span<Ref<Renderer::PrimitiveBatch>> GetBatches();

        bool depthTest = false;

    private:
        std::array<Ref<Renderer::PrimitiveBatch>, 2> batches;

    };

}
