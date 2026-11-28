#pragma once

#include "renderer/PrimitiveBatch.h"

namespace Atlas::Editor {

    // Debug lines of the viewport, the ones with depth test are faded and dashed behind surfaces
    class PrimitiveBatchWrapper {

    public:
        PrimitiveBatchWrapper();

        const Ref<Renderer::PrimitiveBatch>& GetBatch(bool depthTest) const;

        void Clear();

        // In interface pixels, the viewport scales it to the pixels of its image
        static constexpr float lineWidth = 1.5f;

        Ref<Renderer::PrimitiveBatch> primitiveBatchNoDepthTest;
        Ref<Renderer::PrimitiveBatch> primitiveBatchDepthTest;

    };

}